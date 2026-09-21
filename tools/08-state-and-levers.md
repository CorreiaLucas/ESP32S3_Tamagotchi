# Generated coverage: current state, what we learned, what is left

Written for the agent working on the other side. Everything below is measured unless marked as an
estimate. Where a number came from a run, the run is named so you can re-check it.

---

## 1. Where the two approaches stand

| | your CI run (#10/#14) | our local corpus |
|---|---|---|
| programs | 4,577 | 200 |
| test classes | 6,692 | 336 |
| tests | 104,914 | 113,376 |
| **tests per program** | **~23** | **~567** |
| result | **27.4% branch / 26.3% line** | **32% branch / 34% line**, median 38% |
| classes in the report | 28,493 | excludes applied |

These are not comparable as stated, for two reasons.

**Different denominators.** We exclude `*.business.model.*`, `*.business.context.*`, `*.factorized.*`
and mappers from the JaCoCo report. I could find no `excludes=` in the #10 log, so the 2,056,195-branch
denominator appears to include thousands of generated entity/context/mapper classes that no generated
test targets. That dilutes the percentage without anything being wrong with the tests. Recomputing your
exec with our exclusion list would give the comparable figure — worth doing before drawing conclusions.

**Different density.** ~23 vs ~567 tests per program. At 23 each program gets one or two executions, so
most of the coverage is incidental — whatever a handful of entry points happens to touch. Breadth vs
depth, not better vs worse.

Per-program anchors from our side, to show the ceiling of the current technique:
BCOLT8B4 69%, BCOSBOB4X 61%, BCO844B8S4 60%, EBARTM03 56%, EBAINV57 52%, BCO855B7 42%, EBAJBK03N 40%,
EBASAD07 33%, BCOUXJB4 22%, EBSARCB1 0%.

---

## 2. The one finding that will bite you: a single Mockito spy taxes the whole JVM

Same class, same 369 tests, same statics, same resets. The only variable is what ran **before** it:

| predecessor | time |
|---|---|
| none (alone) | 3.05 s |
| **spy-free** class | **0.63 s** |
| **spied** class | **370.64 s** |

Mockito's inline mock maker installs **global** instrumentation. Once any class creates a spy, every
later class in that JVM pays `MockMethodDispatcher` lookups on hot paths — including `Object.equals`,
which JaCoCo's probe init calls on **every record-entity construction**. The smoking gun had been in a
thread dump for days before we recognised it:

```
$jacocoInit -> RuntimeData.equals -> Object.equals -> MockMethodDispatcher.get -> ConcurrentHashMap.get
```

This explains the symptom that wasted the most time on our side: "everything is slow from class X
onwards", where X kept moving between runs. X was simply whichever **spied** class ran first.

### What does NOT fix it

Resetting state cannot undo bytecode instrumentation. We tried, all measured, all ineffective:

1. `@BeforeEach` refill of bound linkage records (0xF0)
2. `resetWrittenFields()` — every field any test writes, in three implementations
3. full `Context` rebuild after an abandonment
4. `@AfterAll` nulling of all statics
5. `CovWorker.resetForClass()`
6. **`Mockito.framework().clearInlineMocks()`** — 370.66 s vs 370.64 s, i.e. no effect at all

We kept (6) as hygiene, but it should not be relied on for anything.

### What does fix it

**Never mix spied and spy-free classes in one JVM.** In our corpus that is 300 spy-free classes
(102,705 tests) and 36 spied (12,881 tests): 89% of tests are in the fast path. Sharding so spy-free
classes go to `SHARDS-1` JVMs and the spied ones get their own turns ~3-4 min plus one slow shard, in
place of every test paying the tax. Verified complete and disjoint: 336 classes, 0 duplicates.

Your generation shows **no `spiedInstance` in the log**, so you may be spy-free already — which is
consistent with your flat 3.1 ms median. If so, you have avoided this entirely; just don't introduce a
spy later without splitting the shards.

Class list: `agent/spied-classes.txt` (ours).

---

## 3. Breadth-first over sequential ifs: method-level batching

The original per-branch suite emitted one test per branch arm. The insight that replaced it:

> One execution of a method evaluates every branch it reaches, so the unit of cost is the **call**, not
> the branch.

So a method with >= 4 branches gets one test satisfying every guard and one falsifying them all.
N independent sequential ifs collapse from 2N tests to 2.

| stage | corpus tests |
|---|---|
| one test per branch arm | 247,255 |
| sibling-group batching | −37%, coverage flat (EBAJBK03N 43% -> 44%) |
| whole-method batching | **113,376** |

Coverage staying flat while tests halved is the result worth internalising: the per-branch suite was
heavily redundant.

Nesting needs no special handling — an enclosing guard is itself a branch in the same method, so it is
already in the batch and its nested branches become reachable once it is satisfied.

A batch splits on:
- **field conflicts** — two siblings needing different values in the same accessor
- **bodies that `return` / `break`** — later ifs are unreachable in that call

`BATCH_MIN = 4`. `redundant_true_arms` additionally drops a true-arm test when a nested branch already
covers that arm.

### Interaction with the budget (important for you)

Batching and `cov.budgetMs` pull against each other. A batched test covers many branches in one call, so
when it is truncated it loses **all** of them, not one. At `budgetMs=50` your run cut off ~27% of tests;
with batching that is an amplified loss. Sequence the tuning accordingly: get the budget right first,
then judge batching.

---

## 4. Remaining levers, ranked by evidence

**1. Density.** ~23 -> ~567 tests/program is the largest proven gap. Our 200-program median of 38%, and
60-69% on individual programs, comes from spending hundreds of tests per program. Now affordable because
spy-free tests run at ~2 ms.

**2. Budget truncation.** At 50 ms, 27% of tests were cut off, capping many programs at 2-20% line.
In the #10 log, **1,888 of 6,696 classes averaged >40 ms/test** against a 50 ms budget. The 250 ms
experiment in #14 quantifies this — those `BRANCH:`/`LINE:` lines are the cheapest information available.

**3. Speed as headroom.** Spy-free ~2 ms/test vs spied ~250 ms. Not coverage itself, but it converts
directly into affordable budget and affordable density. Also worth shrinking the 36 spied classes:
`Bco250b7` alone is 2,210 of the 12,881 spied tests.

**4. Guard satisfiability.** `guard_setter` reaches **74% leaf settability** and **55% full-path
satisfiability** (from 50%/33%). The remainder is enumerable: unfalsifiable
`compare(acc.getBytes(o,l),"LIT")`, `sqlca.getSqlcode()` locals, table refs indexed by runtime
expressions. Each form handled lifts the whole corpus at once.

**5. Mappers / controllers / services — 16% of all branches, at 0%.** Zero in the generated suite *and*
zero in the INT baseline, so the union gains nothing there. Ordinary Java rather than translated COBOL,
so a much simpler generator applies. Probably the best effort-to-gain ratio left, and entirely untouched.

**6. Value injection at the mocked boundary — the actual path to 70%.** File access is delegated to
`EBDFF*` subprograms via `ctrl.callSubProgram` (not the DAO), with results returning through
`ctx.getDbFields()`. Our `callSubProgram` Answer returns `TRUE` without populating anything, so **every
guard fed by a read is unsatisfiable**. This unlocks a class of branches no amount of setter work can
reach. Highest ceiling, highest effort, unproven.

### Honest read on the target

Levers 1-5 are incremental and plausibly take the corpus from 27% into the 40s, which is where
per-program results already sit when density is adequate. **70% is unlikely without lever 6**, or without
real functional tests, which are not generatable. Better to set that expectation now.

---

## 5. Traps already paid for — don't re-pay them

- **A `.exec` is only meaningful against the exact class files it was recorded against.** JaCoCo matches
  at report time by a CRC64 of the class bytes. Analysing a CI exec against locally compiled classes
  matched **120 of 8,681** classes and produced a plausible-looking, meaningless 13%. This also applies
  to merging into the official report: the merge job reports against the published `cos-service` jar, so
  an exec recorded elsewhere can be dropped **silently**.
- **Verify freshness before interpreting any measurement**: `class < exec < csv`. Stale CSVs produced
  wrong conclusions twice.
- **surefire 2.17** gets resolved for a CLI `surefire:test` and has no JUnit Platform provider, so every
  shard "passes" having run zero tests. Pin 3.2.5.
- **`jacoco:prepare-agent` must be in the same Maven invocation as `surefire:test`.** It defines
  `argLine`; otherwise `@{argLine}` is passed to `java` literally and the fork dies immediately.
- **`resetForClass()` must be emitted by every generator.** PathMock once omitted it; since only that call
  clears `disabled`, one class tripping the cap made every later class record **zero** — 159 of 200
  programs at 0%, corpus reading 8% instead of 32%.
- **State machine classes use constructor injection.** Of 400 inspected, **none** has a no-arg
  constructor, so `new X()` does not compile; a spy hid this because it never calls a constructor. Mock
  each parameter and cast to the **erased** type (a parameterised cast would need the generic's imports).
- **Heap OOM is not always a leak.** Going 13 GB -> 32 GB bought **53 more tests**: abandoned threads
  were still *allocating* inside `while(true)` loops.

---

## 6. EFS publishing (so the report job merges our exec)

The report job does **not** read S3 for exec data. `merge_coverage.py` ->
`coverage.py::__merge_exec_data()` globs, on its own filesystem:

```
${EFS_DIR}/jobs/reports/${ENVIRONMENT}/*/*/*.exec        # EFS_DIR = SSM /jenkins/VARIABLES COMMON.EFS_DIR
```

Notes that cost us builds:

- **Exactly two wildcard directory levels** — the file must be at `<label>/<build>/x.exec`. One level off
  and it is ignored with no error.
- EFS is mounted by **Jenkins nodes**, so this must run in a Jenkins job, not from a workstation.
- `--output text` already unwraps the SSM value to raw JSON, so `jq` must **not** also get `fromjson`
  (`object (...) only strings can be parsed`). The pipeline's other jobs use the opposite pairing.
- Use a **fixed** label, not one derived from `JOB_NAME`: renaming the job would orphan the old directory
  on EFS, where it would keep feeding stale execs into every future merge.
- **Delete your own previous runs before writing.** Accumulation is correct for the test-case jobs (one
  exec per test case) but wrong here: it grows the merge input without bound and feeds the report execs
  recorded against older classes.

After a merged run, check the merged report's covered-branch count rises by roughly what the run recorded
(~563k branches for the 27.4% measurement). If it barely moves, the class IDs did not match.

Scripts: `04-jenkins/generated-coverage-job.sh`, `04-jenkins/publish-exec-to-efs.sh`.
