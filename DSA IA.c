#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_REGIONS 100
#define HASH_SIZE 211
#define NAME_LEN 50

typedef struct {
    int id;
    char region[NAME_LEN], city[NAME_LEN];
    float temperature;
    int humidity, heatIndex;
    char risk[16];
} WeatherRecord;

typedef struct AVLNode {
    int index, height;
    struct AVLNode *left, *right;
} AVLNode;

typedef struct {
    int items[MAX_REGIONS], size;
} MaxHeap;

WeatherRecord records[MAX_REGIONS];
int recordCount = 0, hashTable[HASH_SIZE];
AVLNode *root = NULL;
MaxHeap alerts;

int max(int a, int b) { return a > b ? a : b; }
int height(AVLNode *n) { return n ? n->height : 0; }
int balance(AVLNode *n) { return n ? height(n->left) - height(n->right) : 0; }

int compareRecords(int a, int b) {
    if (records[a].temperature != records[b].temperature)
        return records[a].temperature < records[b].temperature ? -1 : 1;
    return records[a].id - records[b].id;
}

AVLNode *newNode(int index) {
    AVLNode *n = malloc(sizeof(AVLNode));
    if (!n) { printf("Memory allocation failed.\n"); exit(1); }
    n->index = index;
    n->height = 1;
    n->left = n->right = NULL;
    return n;
}

AVLNode *rotateRight(AVLNode *y) {
    AVLNode *x = y->left, *t = x->right;
    x->right = y;
    y->left = t;
    y->height = 1 + max(height(y->left), height(y->right));
    x->height = 1 + max(height(x->left), height(x->right));
    return x;
}

AVLNode *rotateLeft(AVLNode *x) {
    AVLNode *y = x->right, *t = y->left;
    y->left = x;
    x->right = t;
    x->height = 1 + max(height(x->left), height(x->right));
    y->height = 1 + max(height(y->left), height(y->right));
    return y;
}

AVLNode *insertAVL(AVLNode *n, int index) {
    if (!n) return newNode(index);

    if (compareRecords(index, n->index) < 0) n->left = insertAVL(n->left, index);
    else n->right = insertAVL(n->right, index);

    n->height = 1 + max(height(n->left), height(n->right));
    int b = balance(n);

    if (b > 1 && compareRecords(index, n->left->index) < 0) return rotateRight(n);
    if (b < -1 && compareRecords(index, n->right->index) > 0) return rotateLeft(n);
    if (b > 1) { n->left = rotateLeft(n->left); return rotateRight(n); }
    if (b < -1) { n->right = rotateRight(n->right); return rotateLeft(n); }
    return n;
}

void freeAVL(AVLNode *n) {
    if (!n) return;
    freeAVL(n->left);
    freeAVL(n->right);
    free(n);
}

void printRecord(int i) {
    WeatherRecord r = records[i];
    printf("%-4d %-20s %-20s %8.1f %8d%% %8d %-10s\n",
           r.id, r.region, r.city, r.temperature,
           r.humidity, r.heatIndex, r.risk);
}

void printHeader(void) {
    printf("%-4s %-20s %-20s %8s %9s %8s %-10s\n",
           "ID", "Region", "City", "Temp C", "Humidity", "Index C", "Risk");
    printf("-------------------------------------------------------------------------------\n");
}

void displayAVL(AVLNode *n) {
    if (!n) return;
    displayAVL(n->left);
    printRecord(n->index);
    displayAVL(n->right);
}

int hashFunction(const char *s) {
    unsigned long h = 5381;
    while (*s) h = h * 33 + (unsigned char)tolower((unsigned char)*s++);
    return (int)(h % HASH_SIZE);
}

int sameRegion(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return 0;
    }
    return *a == *b;
}

int searchHash(const char *region) {
    int start = hashFunction(region);
    for (int i = 0; i < HASH_SIZE; i++) {
        int slot = (start + i) % HASH_SIZE;
        if (hashTable[slot] == -1) return -1;
        if (sameRegion(records[hashTable[slot]].region, region)) return hashTable[slot];
    }
    return -1;
}

int insertHash(int index) {
    int start = hashFunction(records[index].region);
    for (int i = 0; i < HASH_SIZE; i++) {
        int slot = (start + i) % HASH_SIZE;
        if (hashTable[slot] == -1) { hashTable[slot] = index; return 1; }
    }
    return 0;
}

int riskRank(const char *risk) {
    if (strcmp(risk, "Extreme") == 0) return 4;
    if (strcmp(risk, "High") == 0) return 3;
    if (strcmp(risk, "Moderate") == 0) return 2;
    return 1;
}

int higherPriority(int a, int b) {
    int ra = riskRank(records[a].risk), rb = riskRank(records[b].risk);
    if (ra != rb) return ra > rb;
    if (records[a].heatIndex != records[b].heatIndex)
        return records[a].heatIndex > records[b].heatIndex;
    return records[a].id < records[b].id;
}

void swap(int *a, int *b) { int t = *a; *a = *b; *b = t; }

void pushHeap(int index) {
    if (alerts.size >= MAX_REGIONS) return;

    int i = alerts.size++;
    alerts.items[i] = index;

    while (i > 0) {
        int p = (i - 1) / 2;
        if (!higherPriority(alerts.items[i], alerts.items[p])) break;
        swap(&alerts.items[i], &alerts.items[p]);
        i = p;
    }
}

int popHeap(MaxHeap *h) {
    if (h->size == 0) return -1;

    int result = h->items[0], i = 0;
    h->items[0] = h->items[--h->size];

    while (1) {
        int left = 2 * i + 1, right = left + 1, best = i;
        if (left < h->size && higherPriority(h->items[left], h->items[best])) best = left;
        if (right < h->size && higherPriority(h->items[right], h->items[best])) best = right;
        if (best == i) break;
        swap(&h->items[i], &h->items[best]);
        i = best;
    }
    return result;
}

void classifyRisk(WeatherRecord *r) {
    if (r->heatIndex >= 45) strcpy(r->risk, "Extreme");
    else if (r->heatIndex >= 40) strcpy(r->risk, "High");
    else if (r->heatIndex >= 35) strcpy(r->risk, "Moderate");
    else strcpy(r->risk, "Low");
}

void addRecord(void) {
    if (recordCount >= MAX_REGIONS) {
        printf("Maximum record limit reached.\n");
        return;
    }

    WeatherRecord r;
    char input[100];
    r.id = recordCount + 1;

    printf("Enter region/state name: ");
    if (!fgets(r.region, sizeof(r.region), stdin)) return;
    r.region[strcspn(r.region, "\n")] = '\0';

    if (strlen(r.region) == 0) { printf("Region cannot be empty.\n"); return; }
    if (searchHash(r.region) != -1) { printf("This region already exists.\n"); return; }

    printf("Enter city name: ");
    if (!fgets(r.city, sizeof(r.city), stdin)) return;
    r.city[strcspn(r.city, "\n")] = '\0';

    if (strlen(r.city) == 0) { printf("City cannot be empty.\n"); return; }

    printf("Enter temperature in Celsius: ");
    if (!fgets(input, sizeof(input), stdin) ||
        sscanf(input, "%f", &r.temperature) != 1 ||
        r.temperature < -20 || r.temperature > 60) {
        printf("Invalid temperature. Enter a value from -20 to 60.\n");
        return;
    }

    printf("Enter humidity (0-100): ");
    if (!fgets(input, sizeof(input), stdin) ||
        sscanf(input, "%d", &r.humidity) != 1 ||
        r.humidity < 0 || r.humidity > 100) {
        printf("Invalid humidity.\n");
        return;
    }

    r.heatIndex = (int)(r.temperature + 0.5f);
    classifyRisk(&r);
    records[recordCount] = r;

    if (!insertHash(recordCount)) { printf("Hash table insertion failed.\n"); return; }

    root = insertAVL(root, recordCount);
    pushHeap(recordCount++);

    printf("\nRecord added successfully.\n");
    printf("Demo heat-index value: %d C\n", r.heatIndex);
    printf("Rule-based risk level: %s\n", r.risk);
}

void displayRecords(void) {
    if (recordCount == 0) { printf("No records available. Add a record first.\n"); return; }
    printf("\nRECORDS SORTED BY TEMPERATURE (AVL TREE)\n");
    printHeader();
    displayAVL(root);
}

void searchRegion(void) {
    char region[NAME_LEN];
    printf("Enter region/state to search: ");
    if (!fgets(region, sizeof(region), stdin)) return;
    region[strcspn(region, "\n")] = '\0';

    int index = searchHash(region);
    if (index == -1) { printf("Region not found.\n"); return; }

    printf("\nRegion found using Hash Table:\n");
    printHeader();
    printRecord(index);
}

void displayAlerts(void) {
    if (recordCount == 0) { printf("No alerts available. Add a record first.\n"); return; }

    MaxHeap copy = alerts;
    printf("\nPRIORITY ALERTS (MAX HEAP)\n");
    printHeader();

    while (copy.size > 0) printRecord(popHeap(&copy));
}

void showMenu(void) {
    printf("\n========================================\n");
    printf("             HEATGUARD SYSTEM\n");
    printf("      Heatwave Monitoring Project\n");
    printf("========================================\n");
    printf("1. Add weather record\n");
    printf("2. Display records (AVL Tree)\n");
    printf("3. Search region (Hash Table)\n");
    printf("4. Display priority alerts (Max Heap)\n");
    printf("5. Exit\n");
}

int main(void) {
    for (int i = 0; i < HASH_SIZE; i++) hashTable[i] = -1;
    alerts.size = 0;

    int choice;
    do {
        showMenu();
        char input[100];
        printf("Enter choice: ");
        if (!fgets(input, sizeof(input), stdin)) break;

        if (sscanf(input, "%d", &choice) != 1 || choice < 1 || choice > 5) {
            printf("Invalid choice. Enter a number from 1 to 5.\n");
            choice = 0;
            continue;
        }

        switch (choice) {
            case 1: addRecord(); break;
            case 2: displayRecords(); break;
            case 3: searchRegion(); break;
            case 4: displayAlerts(); break;
            case 5: printf("Exiting HeatGuard. Stay safe!\n"); break;
        }
    } while (choice != 5);

    freeAVL(root);
    return 0;
}
