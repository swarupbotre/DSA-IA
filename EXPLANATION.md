# HeatGuard — Full Code Walkthrough

A plain-English explanation of `DSA IA.c`: what the program does, why each data
structure is there, and how the pieces connect.

---

## The big picture

It's a **menu-driven console app** that stores weather readings for regions and,
for each, computes a **heat-risk level** and shows it in three different ways:

| Menu option | Data structure | Question it answers |
|---|---|---|
| 1. Add record | — | collect + validate input, then feed all 3 structures |
| 2. Display records | **AVL tree** | "show me everything, sorted by temperature" |
| 3. Search region | **Hash table** | "find this one region, fast" |
| 4. Priority alerts | **Max heap** | "what's the most dangerous right now?" |
| 5. Exit | — | free memory, quit |

That's the whole point of the assessment: **one dataset, three structures**,
chosen so each one wins at a different job.

"IA" = *Internal Assessment*. The risk level is **not** produced by a trained AI
model — it comes from plain `if` thresholds (see §3).

---

## 1. The data model

```c
typedef struct {
    int id;
    char region[NAME_LEN], city[NAME_LEN];   // 50 chars each
    float temperature;
    int humidity, heatIndex;
    char risk[16];                           // "Extreme"/"High"/"Moderate"/"Low"
} WeatherRecord;
```

**Key design decision:** there is exactly **one** array of real records, and it's global:

```c
WeatherRecord records[MAX_REGIONS];   // MAX_REGIONS = 100
int recordCount = 0;
```

The AVL tree and the heap do **not** store copies of records. They store **`int`
indices** into `records[]`:

```c
typedef struct AVLNode { int index, height; struct AVLNode *left, *right; } AVLNode;
typedef struct { int items[MAX_REGIONS], size; } MaxHeap;
```

Why this matters: one record, one source of truth. Change `records[5]` and every
structure immediately reflects it. It also keeps the tree/heap cheap (4 bytes per
node instead of a ~150-byte struct), and makes the three orderings independent —
the tree sorts by temperature, the heap by danger, without conflicting.

`HASH_SIZE` is 211 — a prime, which spreads the `% 211` results more evenly.

---

## 2. Adding a record (`addRecord`)

The input pipeline is defensive at every step:

1. **Capacity check** — refuse past 100 records.
2. **Region** — read with `fgets`, strip the `\n` using `strcspn`, reject if empty.
3. **Duplicate check** — `searchHash(r.region) != -1` → "This region already
   exists." Case-insensitive, so `delhi` collides with `Delhi`.
4. **City** — same empty check.
5. **Temperature** — parse with `sscanf`, must be `[-20, 60]`.
6. **Humidity** — must be `[0, 100]`.
7. **Derive the rest** (see §3).
8. **Store, then wire up all three structures:**

```c
records[recordCount] = r;
if (!insertHash(recordCount)) { ...return; }
root = insertAVL(root, recordCount);
pushHeap(recordCount++);
```

Every rejection prints why and `return`s early — no partial record is ever
stored, because `records[recordCount]` is only written at the very end.

`r.id = recordCount + 1`, so IDs are 1, 2, 3… in creation order.

---

## 3. The "risk classifier" — thresholds, not AI

```c
r.heatIndex = (int)(r.temperature + 0.5f);   // rounded temperature, a demo proxy
classifyRisk(&r);
```

The heat index here is deliberately a **simplified placeholder** — the real heat
index is a messy function of temperature *and* humidity, and this program uses
only a rounded temperature instead.

```c
void classifyRisk(WeatherRecord *r) {
    if (r->heatIndex >= 45)      strcpy(r->risk, "Extreme");
    else if (r->heatIndex >= 40) strcpy(r->risk, "High");
    else if (r->heatIndex >= 35) strcpy(r->risk, "Moderate");
    else                         strcpy(r->risk, "Low");
}
```

Example: `44.6` → `(int)(45.1)` → `45` → **Extreme**.

---

## 4. Structure A — the AVL tree (sorted display)

**Why AVL:** option 2 must print all records in temperature order. Dumping an
array and sorting each time is wasteful; an AVL tree keeps itself balanced, so
insertion stays O(log n) *and* an in-order traversal comes out sorted for free.

```c
int compareRecords(int a, int b) {
    if (records[a].temperature != records[b].temperature)
        return records[a].temperature < records[b].temperature ? -1 : 1;
    return records[a].id - records[b].id;   // tie-break: older record first
}
```

Its components:

- `height()` / `balance()` — a node's height is 1 + max child height; balance
  factor is left-minus-right.
- `rotateRight` / `rotateLeft` — pointer surgery that fixes imbalance while
  preserving ordering; height fields are recalculated after the swap.
- `insertAVL` — recursive BST insert, then rebalance. Four cases:
  - `b > 1` and new key goes left-left → one right rotation
  - `b < -1` and right-right → one left rotation
  - `b > 1` otherwise → left rotation on the child, then right rotation (LR case)
  - `b < -1` otherwise → right on child, then left (RL case)
- `displayAVL` — in-order (left → node → right), which outputs ascending
  temperature. On AVL insert, `root = insertAVL(root, ...)` is reassigned because
  rotations can change which node is the root.
- `freeAVL` — post-order free.

**Why option 2's order is "by temperature" and option 4's isn't:** the tree's
*only* ordering key is temperature. The heap's is danger. Two different
structures, two different questions.

---

## 5. Structure B — the hash table (region lookup)

Option 3 needs a fast exact lookup by name, so: an **open-addressing table with
linear probing**.

```c
int hashFunction(const char *s) {
    unsigned long h = 5381;
    while (*s) h = h * 33 + (unsigned char)tolower((unsigned char)*s++);
    return (int)(h % HASH_SIZE);
}
```

- That's the classic **djb2** hash. `tolower` on every char makes the hash — and
  therefore the lookup — **case-insensitive**: `"Delhi"` and `"delhi"` land in
  the same bucket.
- `hashTable` is initialised to `-1` in `main()`, which is the **empty slot
  sentinel** (indices are always ≥ 0).
- `searchHash` probes `(start + i) % HASH_SIZE`; hit an empty slot → the key
  can't be present → `-1`. Because there are **no deletions** in this program,
  that early exit is safe and no "tombstone" markers are needed.
- `sameRegion` is a hand-written case-insensitive string compare — deliberately
  not `strcasecmp`, which isn't standard C.
- `insertHash` walks the same probe sequence and claims the first `-1`.

Average lookup/insert is O(1); worst case (everything colliding) O(n). With 211
slots and ≤100 records, collisions are rare and probing stays short.

---

## 6. Structure C — the max heap (priority alerts)

Option 4 prints records **most dangerous first**, without sorting anything:

```c
int higherPriority(int a, int b) {
    int ra = riskRank(records[a].risk), rb = riskRank(records[b].risk);
    if (ra != rb) return ra > rb;                                  // 1. risk level
    if (records[a].heatIndex != records[b].heatIndex)
        return records[a].heatIndex > records[b].heatIndex;        // 2. hotter wins
    return records[a].id < records[b].id;                          // 3. older wins
}
```

`riskRank`: Extreme=4, High=3, Moderate=2, everything else=1. So it's a
three-level tie-breaker — risk, then heat index, then age.

- The heap is a **binary max-heap stored in a plain array**: for index `i`,
  children are `2i+1` and `2i+2`, parent is `(i-1)/2`.
- `pushHeap` — append at the end, then **sift up** while the new item outranks
  its parent.
- `popHeap` — remember `items[0]` (the max), move the last element to the root,
  then **sift down** comparing against both children until it belongs.

**The subtlest line in the program:**

```c
MaxHeap copy = alerts;          // whole struct copied, including items[]
while (copy.size > 0) printRecord(popHeap(&copy));
```

`popHeap` is **destructive** — draining it would empty the real alert queue, so
option 4 would only ever work once. Copying the struct (which copies the array
by value, since C copies array members wholesale inside structs) lets it be
destroyed on a throwaway while `alerts` stays intact.

Note the heap array is never physically sorted — it only *guarantees* the root
is the highest priority, which is exactly what repeated `popHeap` needs.

---

## 7. Output formatting

All three views share one header helper (`printHeader`), printing an 80-char
dashed rule and the same column labels:

```c
printf("%-4s %-20s %-20s %8s %9s %8s %-10s\n",
       "ID", "Region", "City", "Temp C", "Humidity", "Index C", "Risk");
```

`%-20s` = left-aligned in 20 chars; `%8.1f` = right-aligned float with 1 decimal;
`%%` = a literal `%` after the humidity value. `printRecord(int i)` was extracted
so the AVL walk, the hash lookup, and the heap drain all render a row identically.

---

## 8. The main loop

```c
for (int i = 0; i < HASH_SIZE; i++) hashTable[i] = -1;   // empty sentinels
alerts.size = 0;
do {
    showMenu();
    if (!fgets(input, sizeof(input), stdin)) break;      // Ctrl+D ends cleanly
    if (sscanf(input, "%d", &choice) != 1 || choice < 1 || choice > 5) {
        printf("Invalid choice. Enter a number from 1 to 5.\n");
        choice = 0; continue;                            // loops instead of exiting
    }
    switch (choice) { case 1: addRecord(); ... case 5: printf("Exiting..."); }
} while (choice != 5);
freeAVL(root);
```

Reading the choice with `fgets` + `sscanf` instead of bare `scanf("%d")` is the
important habit: `scanf` would leave garbage in the buffer on bad input and
desync every later prompt. This way, `"abc"` prints an error and re-shows the menu.

---

## 9. Complexity cheat-sheet

| Operation | Structure | Cost |
|---|---|---|
| Add record | AVL insert | O(log n) |
| Add record | hash insert (avg) | O(1) |
| Add record | heap push | O(log n) |
| Display sorted | AVL in-order | O(n) |
| Search region | hash (avg / worst) | O(1) / O(n) |
| Show alerts | n × heap pop | O(n log n) |

---

## 10. Talking points / honest critique

Good things to say if questioned:

- One array of records + indices elsewhere → single source of truth, no sync bugs.
- Every input path validates and returns before mutating state.
- The heap copy shows real understanding of destructive operations.
- The absence of deletion is why linear probing needs no tombstones — simple and correct.

Fair criticisms to be ready for:

- **Massive global state.** `records`, `root`, `alerts`, `recordCount` are all
  file-scope, and `higherPriority`/`compareRecords` read `records` implicitly
  instead of taking parameters. Works fine here, but hard to test or reuse.
- **No deletion anywhere**, so records can only ever be appended. That's a
  missing feature, not a design win.
- **Heat index is a placeholder**, not the real formula.
- **Fixed-size arrays** (`MAX_REGIONS`, `HASH_SIZE`) mean hard limits instead of
  dynamic growth.
- The hash table needs a load factor below 1; at 100 records in 211 slots it's
  fine, but it isn't scale-proof.

If you follow the paper trail — *input → `records[]` → three index-based
structures → three views* — the program stops looking like one long file and
becomes three classic structures on a shared dataset.
