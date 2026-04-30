#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <unordered_map>
#include <cstdlib>
#include <chrono>
#include <random>
#include <climits>
#include <stdexcept>

struct Element{
    int id;
    int priority;
};

struct PriorityQueueHeap{
    std::vector<Element> heap;

    // mapa jest używana do znajdywania danego elementu - niezbędne do funkcji changePriority()
    std::unordered_map<int, int> pos;

    int parent(int i){ return (i-1)/2; }
    int leftChild(int i){ return 2*i + 1; }
    int rightChild(int i){ return 2*i + 2; }
    int getSize(){ return heap.size(); }

    void restoreOrderPop(int index){
        int maxIndex = index;
        int l = leftChild(index);
        int r = rightChild(index);

        if(l < heap.size() && heap[l].priority > heap[maxIndex].priority){
            maxIndex = l;
        }
        if(r < heap.size() && heap[r].priority > heap[maxIndex].priority){
            maxIndex = r;
        }

        if(index != maxIndex){
            std::swap(heap[index], heap[maxIndex]);

            // hashmap update
            pos[heap[index].id] = index;
            pos[heap[maxIndex].id] = maxIndex;

            restoreOrderPop(maxIndex);
        }
    }

    void restoreOrderPush(int index){
        while(index > 0 && heap[index].priority > heap[parent(index)].priority){
            std::swap(heap[index], heap[parent(index)]);

            // hashmap update
            pos[heap[index].id] = index;
            pos[heap[parent(index)].id] = parent(index);

            index = parent(index);
        }
    }

    void pop(){
        if(heap.empty()){
            throw std::out_of_range("Queue is empty");
        }
        // hashmap update
        pos.erase(heap[0].id);

        heap[0] = heap.back();
        heap.pop_back();

        if(!heap.empty()){
            // hashmap update
            pos[heap[0].id] = 0;

            restoreOrderPop(0);
        }
    }

    void push(int newId, int newPriority){
        if(pos.find(newId) != pos.end()){
            throw std::invalid_argument("Element already exists");
        }

        Element newElement = {newId, newPriority};
        heap.push_back(newElement);

        // hashmap update
        pos[newId] = heap.size() - 1;

        restoreOrderPush(heap.size()-1);
    }

    Element peek(){
        if(heap.empty()){
            throw std::out_of_range("Queue is empty");
        }
        return heap.front();
    }

    void changePriority(int targetID, int newPriority){
        auto it = pos.find(targetID);

        if(it == pos.end()){
            throw std::out_of_range("Element does not exist");
        }

        int index = it->second;

        int prevPriority = heap[index].priority;
        heap[index].priority = newPriority;

        if(prevPriority > newPriority){
            restoreOrderPop(index);
        }
        else if(prevPriority < newPriority){
            restoreOrderPush(index);
        }
        else return;
    }
};

struct PriorityQueueVector{
    std::vector<Element> data;

    int getSize(){ return data.size(); }

    void push(int id, int priority){
        data.push_back({id, priority});
    }

    int findMaxIndex(){
        if(data.empty()){
            throw std::out_of_range("Queue is empty");
        }
        int maxIdx = 0;
        for(int i = 1; i < (int)data.size(); ++i){
            if(data[i].priority > data[maxIdx].priority){
                maxIdx = i;
            }
        }
        return maxIdx;
    }

    Element peek(){
        return data[findMaxIndex()];
    }

    void pop(){
        int maxIdx = findMaxIndex();
        std::swap(data[maxIdx], data.back());
        data.pop_back();
    }

    void changePriority(int id, int newPriority){
        for(int i = 0; i < (int)data.size(); ++i){
            if(data[i].id == id){
                data[i].priority = newPriority;
                return;
            }
        }
        throw std::out_of_range("Element not found");
    }
};

struct Node{
    Element data;
    Node* next;
    Node(Element e) : data(e), next(nullptr) {}
};

struct PriorityQueueSortedList{
    Node* head = nullptr;
    int size = 0;

    int getSize(){ return size; }

    void push(int id, int priority){
        Node* newNode = new Node({id, priority});
        // wstaw zachowujac porzadek malejacy (max na poczatku)
        if(head == nullptr || priority > head->data.priority){
            newNode->next = head;
            head = newNode;
        } else {
            Node* current = head;
            while(current->next != nullptr && current->next->data.priority >= priority){
                current = current->next;
            }
            newNode->next = current->next;
            current->next = newNode;
        }
        size++;
    }

    Element peek(){
        if(head == nullptr){
            throw std::out_of_range("Queue is empty");
        }
        return head->data;
    }

    void pop(){
        if(head == nullptr){
            throw std::out_of_range("Queue is empty");
        }
        Node* tmp = head;
        head = head->next;
        delete tmp;
        size--;
    }

    void changePriority(int id, int newPriority){
        Node* prev = nullptr;
        Node* current = head;
        while(current != nullptr){
            if(current->data.id == id){
                if(prev == nullptr) head = current->next;
                else prev->next = current->next;
                delete current;
                size--;
                push(id, newPriority);
                return;
            }
            prev = current;
            current = current->next;
        }
        throw std::out_of_range("Element not found");
    }

    ~PriorityQueueSortedList(){
        while(head != nullptr){
            Node* tmp = head;
            head = head->next;
            delete tmp;
        }
    }
};

static const int SAMPLES = 1000;

// volatile sink - zapobiega usuwaniu wywolania peek() przez optymalizator
// (kompilator nie moze udowodnic ze odczyt jest "martwy", bo zapis do volatile to side effect)
static volatile int g_sink = 0;

template<typename F>
long long bench(F op){
    auto start = std::chrono::high_resolution_clock::now();
    for(int j = 0; j < SAMPLES; ++j) op(j);
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / SAMPLES;
}

// Wersja dla bardzo szybkich operacji (peek O(1)) - wiecej iteracji + double dla sub-ns precyzji.
template<typename F>
double benchND(int samples, F op){
    auto start = std::chrono::high_resolution_clock::now();
    for(int j = 0; j < samples; ++j) op(j);
    auto end = std::chrono::high_resolution_clock::now();
    long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return (double)ns / samples;
}

void writeHeader(std::ofstream& f, const std::string& s){
    std::cout << s << std::endl;
    f << s << "\n";
}

void writeRow(std::ofstream& f, int N, long long t){
    std::cout << "N=" << N << " ; OPTIME=" << t << std::endl;
    f << "N=" << N << " ; OPTIME=" << t << "\n";
}

void writeRowD(std::ofstream& f, int N, double t){
    std::cout << "N=" << N << " ; OPTIME=" << std::fixed << std::setprecision(2) << t << std::endl;
    f << "N=" << N << " ; OPTIME=" << std::fixed << std::setprecision(2) << t << "\n";
}

void test_vector(std::ofstream& f){
    std::vector<int> Ns = {1000, 10000, 100000, 1000000, 10000000};
    std::mt19937 rng(42);

    f << "\n================ VECTOR (nieposortowany) ================\n";
    std::cout << "\n================ VECTOR ================\n";

    // push: zawsze O(1) amortyzowane (push_back). Best = avg = worst.
    writeHeader(f, "---- push() (best=avg=worst, O(1) amortyzowane) ----");
    for(int N : Ns){
        PriorityQueueVector pq;
        for(int i = 0; i < N; ++i) pq.push(i, i);
        double t = benchND(100000, [&](int j){ pq.push(N + j, j); });
        writeRowD(f, N, t);
    }

    // pop: findMaxIndex zawsze pelny skan -> O(N). Brak roznicy best/worst.
    writeHeader(f, "---- pop() (best=avg=worst, O(N) - findMaxIndex pelny skan) ----");
    for(int N : Ns){
        PriorityQueueVector pq;
        for(int i = 0; i < N; ++i) pq.push(i, i);
        long long t = bench([&](int){ pq.pop(); });
        writeRow(f, N, t);
    }

    // peek: tak samo O(N).
    writeHeader(f, "---- peek() (best=avg=worst, O(N)) ----");
    for(int N : Ns){
        PriorityQueueVector pq;
        for(int i = 0; i < N; ++i) pq.push(i, i);
        // dla N>=100k 1000 sampli wystarczy, dla mniejszych N peek jest szybki
        int samples = (N >= 100000) ? 1000 : 100000;
        double t = benchND(samples, [&](int){ g_sink = pq.peek().id; });
        writeRowD(f, N, t);
    }

    // changePriority: liniowe wyszukiwanie po id - zalezy od pozycji.
    // Po push(i, i) element o id=k jest w data[k].
    writeHeader(f, "---- changePriority() best (id=0, na poczatku - O(1)) ----");
    for(int N : Ns){
        PriorityQueueVector pq;
        for(int i = 0; i < N; ++i) pq.push(i, i);
        double t = benchND(1000000, [&](int j){ pq.changePriority(0, j); });
        writeRowD(f, N, t);
    }

    writeHeader(f, "---- changePriority() avg (id losowe - srednio O(N/2)) ----");
    for(int N : Ns){
        PriorityQueueVector pq;
        for(int i = 0; i < N; ++i) pq.push(i, i);
        std::vector<int> targets(SAMPLES), prios(SAMPLES);
        std::uniform_int_distribution<int> distId(0, N-1);
        std::uniform_int_distribution<int> distP(0, 2*N);
        for(int j = 0; j < SAMPLES; ++j){
            targets[j] = distId(rng);
            prios[j] = distP(rng);
        }
        long long t = bench([&](int j){ pq.changePriority(targets[j], prios[j]); });
        writeRow(f, N, t);
    }

    writeHeader(f, "---- changePriority() worst (id=N-1, na koncu - O(N)) ----");
    for(int N : Ns){
        PriorityQueueVector pq;
        for(int i = 0; i < N; ++i) pq.push(i, i);
        long long t = bench([&](int j){ pq.changePriority(N-1, j); });
        writeRow(f, N, t);
    }
}

void test_sortedlist(std::ofstream& f){
    // N ograniczone do 100k - dla scenariusza "avg push" trzeba miec liste z losowymi
    // priorytetami, a fill takiej listy jest O(N^2) (kazdy push wymaga przejscia ~N/2 nodow).
    // Dla N=1M fill zajmuje godziny. 100k to ~5s i jest do wytrzymania.
    std::vector<int> Ns = {1000, 10000, 100000};
    std::mt19937 rng(42);

    f << "\n================ SORTED LIST (jednokierunkowa, malejaca) ================\n";
    f << "(N ograniczone do 100k bo fill listy z losowymi priorytetami jest O(N^2))\n";
    std::cout << "\n================ SORTED LIST ================\n";

    // best: nowy prio > head -> wstawienie w head -> O(1)
    writeHeader(f, "---- push() best (nowy prio > head, wstawienie w head, O(1)) ----");
    for(int N : Ns){
        PriorityQueueSortedList pq;
        // fill push(i, i) ascending - kazdy nowy idzie do head -> fill jest O(N)
        for(int i = 0; i < N; ++i) pq.push(i, i);
        long long t = bench([&](int j){ pq.push(N + j, N + 1 + j); }); // rosnacy prio, zawsze > head
        writeRow(f, N, t);
    }

    // avg: lista z losowymi prio, push z losowym prio -> srednio O(N/2)
    writeHeader(f, "---- push() avg (lista i prio losowe, srednio O(N/2)) ----");
    for(int N : Ns){
        PriorityQueueSortedList pq;
        std::uniform_int_distribution<int> dist(0, 2*N);
        for(int i = 0; i < N; ++i) pq.push(i, dist(rng)); // O(N^2) fill
        std::vector<int> prios(SAMPLES);
        for(int j = 0; j < SAMPLES; ++j) prios[j] = dist(rng);
        long long t = bench([&](int j){ pq.push(N + j, prios[j]); });
        writeRow(f, N, t);
    }

    // worst: nowy prio < tail -> przejscie do konca listy -> O(N)
    writeHeader(f, "---- push() worst (nowy prio < tail, przejscie do konca, O(N)) ----");
    for(int N : Ns){
        PriorityQueueSortedList pq;
        for(int i = 0; i < N; ++i) pq.push(i, i); // fill ascending - O(N), kazdy idzie do head
        long long t = bench([&](int j){ pq.push(N + j, INT_MIN + j); }); // rosnacy malejaco -> kazdy < poprzedniego tail
        writeRow(f, N, t);
    }

    writeHeader(f, "---- pop() (best=avg=worst, O(1)) ----");
    for(int N : Ns){
        PriorityQueueSortedList pq;
        for(int i = 0; i < N; ++i) pq.push(i, i);
        long long t = bench([&](int){ pq.pop(); });
        writeRow(f, N, t);
    }

    writeHeader(f, "---- peek() (best=avg=worst, O(1)) ----");
    for(int N : Ns){
        PriorityQueueSortedList pq;
        for(int i = 0; i < N; ++i) pq.push(i, i);
        double t = benchND(10000000, [&](int){ g_sink = pq.peek().id; });
        writeRowD(f, N, t);
    }

    // best: id w head + nowy prio > all -> O(1) szukanie + O(1) reinsert
    // (po fill push(i,i) ascending: head ma id=N-1 z prio=N-1)
    writeHeader(f, "---- changePriority() best (id=N-1 w head, prio rosnacy, O(1)) ----");
    for(int N : Ns){
        PriorityQueueSortedList pq;
        for(int i = 0; i < N; ++i) pq.push(i, i);
        long long t = bench([&](int j){ pq.changePriority(N-1, 2*N + j); });
        writeRow(f, N, t);
    }

    // avg: id losowe, prio losowe
    writeHeader(f, "---- changePriority() avg (id i prio losowe) ----");
    for(int N : Ns){
        PriorityQueueSortedList pq;
        for(int i = 0; i < N; ++i) pq.push(i, i);
        std::vector<int> targets(SAMPLES), prios(SAMPLES);
        std::uniform_int_distribution<int> distId(0, N-1);
        std::uniform_int_distribution<int> distP(0, 2*N);
        for(int j = 0; j < SAMPLES; ++j){
            targets[j] = distId(rng);
            prios[j] = distP(rng);
        }
        long long t = bench([&](int j){ pq.changePriority(targets[j], prios[j]); });
        writeRow(f, N, t);
    }

    // worst: id w ogonie + nowy prio < min -> O(N) szukanie + O(N) reinsert
    // (po fill push(i,i) ascending: tail ma id=0; nowy prio rosnacy malejaco zeby zostal w ogonie)
    writeHeader(f, "---- changePriority() worst (id=0 w ogonie, prio < min, O(N)) ----");
    for(int N : Ns){
        PriorityQueueSortedList pq;
        for(int i = 0; i < N; ++i) pq.push(i, i);
        long long t = bench([&](int j){ pq.changePriority(0, INT_MIN + j); });
        writeRow(f, N, t);
    }
}

// Pomiary kolegi (kopiec) - skopiowane z poprzedniej wersji results.txt, niezmieniane.
const char* HEAP_RESULTS_LITERAL =
    "\n================ HEAP (kopiec binarny, max-heap) - pomiary kolegi ================\n"
    "---- push() ----\n"
    "N=1000 ; OPTIME=389\n"
    "N=10000 ; OPTIME=613\n"
    "N=100000 ; OPTIME=210\n"
    "N=1000000 ; OPTIME=241\n"
    "N=10000000 ; OPTIME=218\n"
    "\n"
    "---- pop() ----\n"
    "N=1000 ; OPTIME=1075\n"
    "N=10000 ; OPTIME=2258\n"
    "N=100000 ; OPTIME=2171\n"
    "N=1000000 ; OPTIME=2550\n"
    "N=10000000 ; OPTIME=2941\n"
    "\n"
    "---- peek() ----\n"
    "N=1000 ; OPTIME=17\n"
    "N=10000 ; OPTIME=17\n"
    "N=100000 ; OPTIME=17\n"
    "N=1000000 ; OPTIME=17\n"
    "N=10000000 ; OPTIME=17\n"
    "\n"
    "---- changePriority() ----\n"
    "N=1000 ; OPTIME=381\n"
    "N=10000 ; OPTIME=667\n"
    "N=100000 ; OPTIME=237\n"
    "N=1000000 ; OPTIME=582\n"
    "N=10000000 ; OPTIME=655\n";

int main(){
    std::ofstream f("results.txt");
    if(!f.is_open()){
        std::cerr << "Nie mozna otworzyc results.txt" << std::endl;
        return 1;
    }
    f << "Wyniki: chrono::high_resolution_clock, samples=1000, czas usredniony w ns\n";
    f << "Format: N=<rozmiar kolejki przed pomiarem> ; OPTIME=<sredni czas operacji w ns>\n";

    f << HEAP_RESULTS_LITERAL;

    test_vector(f);
    test_sortedlist(f);

    f.close();
    std::cout << "\nWyniki zapisane do results.txt" << std::endl;
    return 0;
}
