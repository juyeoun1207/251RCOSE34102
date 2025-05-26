#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_PROCESSES 5
#define MAX_TIME 100
#define MAX_IO_REQUESTS 3
#define TIME_QUANTUM 2

#define QUEUE_CAPACITY (MAX_PROCESSES+1)

#define FCFS_MODE 0
#define SJF_MODE 1
#define SJF_PREEMPTIVE_MODE 2
#define PRIORITY_MODE 3
#define PRIORITY_PREEMPTIVE_MODE 4
#define RR_MODE 5
#define LOTTERY_MODE 6


//프로세스 상태 정의
typedef enum { NEW, READY, RUNNING, WAITING, TERMINATED } State;

typedef struct {
    int pid;
    int arrival_time;
    int burst_time; //전체 CPU 수행 시간
    int remaining_time; //남은 CPU 수행 시간

    int io_request_times[MAX_IO_REQUESTS]; // I/O요청 시점들 (CPU 실행 기준 시간)
    int io_burst_times[MAX_IO_REQUESTS]; // 각 I/O 작업 시간
    int io_remaining_time;
    int io_done[MAX_IO_REQUESTS]; //각 I/O 요청이 수행되었는지 여부 (0->X, 1->O)

    int priority;
    int start_time;
    int finish_time;
    int waiting_time;
    int turnaround_time;
    int response_time;
    int ticket_count;

    int time_slice_counter;
    State state;
} Process;


// ready, waiting 용도의 큐 구조
typedef struct {
    int items[QUEUE_CAPACITY];
    int front;
    int rear;
} Queue;


Process PCB[MAX_PROCESSES];
Queue jobQueue;
Queue readyQueue;
Queue waitingQueue;
int gantt_chart[MAX_TIME];


// 클론
Process PCB_clone[MAX_PROCESSES];
Queue jobQueue_clone;


// 큐 초기화
void init_queue(Queue* q) {
    q->front = 0;
    q->rear = 0;
}


int is_empty(Queue* q) {
    return q->front == q->rear;
}

int is_full(Queue* q) {
    return (q->rear + 1) % QUEUE_CAPACITY == q->front;
}

int in_queue(Queue* q, int pid) {
    int i = q->front;
    while (i != q->rear) {
        if (q->items[i] == pid) return 1;
        i = (i + 1) % QUEUE_CAPACITY;
    }
    return 0;
}


int dequeue(Queue* q) {
    if (is_empty(q)) {
        printf("Queue is empty\n");
        return -1;
    }

    q->front = (q->front + 1) % QUEUE_CAPACITY;
    return q->items[q->front];
}


void enqueue(Queue* q, int pid) {
    if (is_full(q)) {
        printf("Queue is full\n");
        return;
    }

    q->rear = (q->rear + 1) % QUEUE_CAPACITY;
    q->items[q->rear] = pid;
}


// Priority가 높은 순서대로 큐에 삽입
void enqueue_priority(Queue* q, int pid) {
    if (is_full(q) || in_queue(q, pid)) return;

    int temp[QUEUE_CAPACITY];
    int count = 0;
    int inserted = 0;

    while (!is_empty(q)) {
        int cur = dequeue(q);
        if (!inserted && PCB[pid].priority < PCB[cur].priority) {
            temp[count++] = pid;
            inserted = 1;
        }
        temp[count++] = cur;
    }
    if (!inserted) temp[count++] = pid;

    for (int i = 0; i < count; i++) {
        enqueue(q, temp[i]);
    }
}


// 짧은 CPU burst time 순서대로 큐에 삽입 
void enqueue_sjf(Queue* q, int pid) {
    if (is_full(q) || in_queue(q, pid)) return;

    int temp[QUEUE_CAPACITY];
    int count = 0;
    int inserted = 0;

    // 큐에서 모두 꺼내 정렬 삽입 준비
    while (!is_empty(q)) {
        int cur = dequeue(q);
        if (!inserted && PCB[pid].remaining_time < PCB[cur].remaining_time) {
            temp[count++] = pid;
            inserted = 1;
        }
        temp[count++] = cur;
    }

    // 아직 삽입 안 했으면 맨 뒤에 삽입
    if (!inserted) {
        temp[count++] = pid;
    }

    // 큐에 다시 삽입
    for (int i = 0; i < count; i++) {
        enqueue(q, temp[i]);
    }
}


// 큐에서 특정 프로세스 제거
int remove_from_queue(Queue* q, int pid) {
    if (is_empty(q)) return -1;

    int temp[QUEUE_CAPACITY];
    int count = 0;
    int found = 0;

    // 큐에서 모두 꺼내서 pid가 아닌 애들만 임시 배열에 저장
    while (!is_empty(q)) {
        int val = dequeue(q);
        if (val == pid) {
            found = 1; // 찾음 표시
        }
        else {
            temp[count++] = val;
        }
    }

    // 임시 배열에 저장된 애들 다시 큐에 삽입
    for (int i = 0; i < count; i++) {
        enqueue(q, temp[i]);
    }

    if (found == 1) {
        return pid; // 제거 성공
    }
    else {
        return -1; 
    }
}


// 클론 함수 (백업용)
void clone_state() {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB_clone[i] = PCB[i];
    }

    // jobQueue 복사
    jobQueue_clone.front = jobQueue.front;
    jobQueue_clone.rear = jobQueue.rear;

    int i = jobQueue.front;
    while (i != jobQueue.rear) {
        i = (i + 1) % QUEUE_CAPACITY;
        jobQueue_clone.items[i] = jobQueue.items[i];
    }
}


// 클론 복원 (불러오기용)
void load_clone_state() {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB[i] = PCB_clone[i];

        // 상태 전부 초기화
        PCB[i].state = NEW;
        PCB[i].remaining_time = PCB[i].burst_time;
        PCB[i].waiting_time = 0;
        PCB[i].turnaround_time = 0;
        PCB[i].response_time = -1;
        PCB[i].start_time = -1;
        PCB[i].finish_time = 0;
        PCB[i].time_slice_counter = 0;
        PCB[i].io_remaining_time = 0;
        for (int j = 0; j < MAX_IO_REQUESTS; j++) {
            PCB[i].io_done[j] = 0;
        }
    }

    init_queue(&jobQueue);
    int i = jobQueue_clone.front;
    while (i != jobQueue_clone.rear) {
        i = (i + 1) % QUEUE_CAPACITY;
        enqueue(&jobQueue, jobQueue_clone.items[i]);
    }
}


// -1 또는 삭제된 pid 제거 후 앞으로 땡김
void compact_queue(Queue* q) {
    int temp[QUEUE_CAPACITY];
    int count = 0;

    // 큐에서 유효한 값들을 temp로 일렬 정리
    int i = q->front;
    while (i != q->rear) {
        i = (i + 1) % QUEUE_CAPACITY;
        temp[count++] = q->items[i];
    }

    // 큐를 초기화한 뒤, 정리된 값 다시 삽입
    q->front = 0;
    q->rear = 0;
    for (int j = 0; j < count; j++) {
        q->rear = (q->rear + 1) % QUEUE_CAPACITY;
        q->items[q->rear] = temp[j];
    }
}


// 시간에 따라 jobQueue에서 프로세스를 readyQueue로 이동
void jobqueue_to_readyqueue(int time, int mode) {
    int temp[MAX_PROCESSES];
    int count = 0;

    while (!is_empty(&jobQueue)) {
        int pid = dequeue(&jobQueue);
        if (PCB[pid].arrival_time <= time) {
            PCB[pid].state = READY;
            PCB[pid].time_slice_counter = 0;
            switch (mode) {
            case PRIORITY_MODE:
            case PRIORITY_PREEMPTIVE_MODE:
                enqueue_priority(&readyQueue, pid);
                break;
            case SJF_MODE:
            case SJF_PREEMPTIVE_MODE:
                enqueue_sjf(&readyQueue, pid);
                break;
            default:
                enqueue(&readyQueue, pid);
                break;
            }
            printf("Time %2d: P%d arrived -> READY\n", time, pid);
        }
        else {
            temp[count++] = pid;  // 아직 도착 안한 프로세스는 다시 큐에 넣기
        }
    }
    for (int i = 0; i < count; i++) {
        enqueue(&jobQueue, temp[i]);
    }
    compact_queue(&jobQueue);
}


// 랜덤한 프로세스 생성해서 jobQueue에 삽입
void Create_Processes() {
    srand(time(NULL));
    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB[i].pid = i;
        PCB[i].arrival_time = rand() % 6; // 0~5 도착
        PCB[i].burst_time = rand() % 7 + 6; // 6~12 CPU 시간
        PCB[i].remaining_time = PCB[i].burst_time;

        // I/O 요청 시점이 겹치지 않도록 정렬
        int temp_io_times[MAX_IO_REQUESTS];
        for (int j = 0; j < MAX_IO_REQUESTS; j++) {
            // 각 I/O 요청 시점은 1~(burst_time-2)
            temp_io_times[j] = rand() % (PCB[i].burst_time - 2) + 1;
            PCB[i].io_burst_times[j] = rand() % 3 + 2; // 2~4 I/O 시간
            PCB[i].io_done[j] = 0; // 아직 수행X
        }

        // I/O 요청 시점 정렬 (중복 제거)
        for (int j = 0; j < MAX_IO_REQUESTS - 1; j++) {
            for (int k = j + 1; k < MAX_IO_REQUESTS; k++) {
                if (temp_io_times[j] > temp_io_times[k]) {
                    int temp = temp_io_times[j];
                    temp_io_times[j] = temp_io_times[k];
                    temp_io_times[k] = temp;

                    // 해당하는 burst_time도 함께 교환
                    temp = PCB[i].io_burst_times[j];
                    PCB[i].io_burst_times[j] = PCB[i].io_burst_times[k];
                    PCB[i].io_burst_times[k] = temp;
                }
            }
        }

        // 정렬된 결과를 다시 저장
        for (int j = 0; j < MAX_IO_REQUESTS; j++) {
            PCB[i].io_request_times[j] = temp_io_times[j];
        }

        PCB[i].io_remaining_time = 0;
        PCB[i].priority = rand() % 5 + 1;
        PCB[i].start_time = -1;
        PCB[i].finish_time = 0;
        PCB[i].waiting_time = 0;
        PCB[i].turnaround_time = 0;
        PCB[i].response_time = -1;
        PCB[i].time_slice_counter = 0;
        PCB[i].state = NEW;
        PCB[i].ticket_count = rand() % 20 + 1;

        enqueue(&jobQueue, i);
    }

    // 생성된 프로세스 정보 출력 (디버깅용)
    printf("Generated Processes:\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        printf("P%d: Arrival=%d, Burst=%d, Priority=%d, Tickets=%d\n",
            i, PCB[i].arrival_time, PCB[i].burst_time, PCB[i].priority, PCB[i].ticket_count);
    }
    printf("\n");
}

/*
//테스트용!!!
// 명시된 값으로 프로세스 생성 및 jobQueue에 삽입
void Create_Processes() {
    // 프로세스 정보 명시
    int arrivals[] = { 0, 5, 2, 3, 5 };
    int bursts[] = { 7, 9, 11, 9, 8 };
    int priorities[] = { 3, 2, 1, 5, 1 };
    int io_times[MAX_PROCESSES][MAX_IO_REQUESTS] = {
        {1, 2, 3},
        {1, 2, 6},
        {2, 3, 6},
        {2, 5, 6},
        {3, 4, 5}
    };
    int io_bursts[MAX_PROCESSES][MAX_IO_REQUESTS] = {
        {4, 3, 2}, // P0
        {4, 4, 4}, // P1
        {4, 4, 2}, // P2
        {2, 4, 3}, // P3
        {4, 3, 4}  // P4
    };

    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB[i].pid = i;
        PCB[i].arrival_time = arrivals[i];
        PCB[i].burst_time = bursts[i];
        PCB[i].remaining_time = PCB[i].burst_time;
        PCB[i].priority = priorities[i];
        PCB[i].start_time = -1;
        PCB[i].finish_time = 0;
        PCB[i].waiting_time = 0;
        PCB[i].turnaround_time = 0;
        PCB[i].response_time = -1;
        PCB[i].time_slice_counter = 0;
        PCB[i].state = NEW;
        PCB[i].io_remaining_time = 0;
        for (int j = 0; j < MAX_IO_REQUESTS; j++) {
            PCB[i].io_request_times[j] = io_times[i][j];
            PCB[i].io_burst_times[j] = io_bursts[i][j];
            PCB[i].io_done[j] = 0;
        }
        enqueue(&jobQueue, i);
    }

    // 생성된 프로세스 정보 출력 (확인용)
    printf("Fixed Processes:\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        printf("P%d: Arrival=%d, Burst=%d, Priority=%d, I/O_times=[%d,%d,%d], I/O_bursts=[%d,%d,%d]\n",
            i, PCB[i].arrival_time, PCB[i].burst_time, PCB[i].priority,
            PCB[i].io_request_times[0], PCB[i].io_request_times[1], PCB[i].io_request_times[2],
            PCB[i].io_burst_times[0], PCB[i].io_burst_times[1], PCB[i].io_burst_times[2]);
    }
    printf("\n");
}  */


int FCFS(int running_pid) {
    if (running_pid != -1) {
        return running_pid;
    }

    return dequeue(&readyQueue);
}


int SJF(int running_pid) {
    if (running_pid != -1) {
        return running_pid;
    }

    if (is_empty(&readyQueue)) {
        return -1;
    }

    // 가장 짧은 remaining_time을 가진 프로세스 찾기
    int shortest_pid = -1;
    int min_time = 99999;

    int i = readyQueue.front;
    while (i != readyQueue.rear){
        i = (i + 1) % QUEUE_CAPACITY;
        int pid = readyQueue.items[i];

        if (PCB[pid].remaining_time < min_time ||
            (PCB[pid].remaining_time == min_time && PCB[pid].arrival_time < PCB[shortest_pid].arrival_time) ||
            (PCB[pid].remaining_time == min_time && PCB[pid].arrival_time == PCB[shortest_pid].arrival_time && pid < shortest_pid)) {
            min_time = PCB[pid].remaining_time;
            shortest_pid = pid;
        }
    }

    // 선택된 프로세스를 큐에서 제거
    if (shortest_pid != -1) {
        remove_from_queue(&readyQueue, shortest_pid);
    }

    return shortest_pid;
}


int SJF_Preemptive(int running_pid) {
    if (is_empty(&readyQueue)) {
        return running_pid;
    }

    // 가장 짧은 remaining_time을 가진 프로세스 찾기
    int shortest_pid = -1;
    int min_time = 99999;

    int i = readyQueue.front;
    while (i != readyQueue.rear) {
        i = (i + 1) % QUEUE_CAPACITY;
        int pid = readyQueue.items[i];

        if (PCB[pid].remaining_time < min_time ||
            (PCB[pid].remaining_time == min_time && PCB[pid].arrival_time < PCB[shortest_pid].arrival_time) ||
            (PCB[pid].remaining_time == min_time && PCB[pid].arrival_time == PCB[shortest_pid].arrival_time && pid < shortest_pid)) {
            min_time = PCB[pid].remaining_time;
            shortest_pid = pid;
        }
    }

    // 선점 조건 확인
    if (shortest_pid != -1) {
        if (running_pid == -1 ||
            PCB[shortest_pid].remaining_time < PCB[running_pid].remaining_time ||
            (PCB[shortest_pid].remaining_time == PCB[running_pid].remaining_time && PCB[shortest_pid].arrival_time < PCB[running_pid].arrival_time) ||
            (PCB[shortest_pid].remaining_time == PCB[running_pid].remaining_time && PCB[shortest_pid].arrival_time == PCB[running_pid].arrival_time && shortest_pid < running_pid)) {
            
            if (running_pid != -1) {
                PCB[running_pid].state = READY;
                enqueue(&readyQueue, running_pid);
            }
            remove_from_queue(&readyQueue, shortest_pid);
            return shortest_pid;
        }
    }

    return running_pid;
}


int Priority(int running_pid) {
    if (running_pid != -1) {
        return running_pid;
    }

    if (is_empty(&readyQueue)) {
        return -1;
    }

    int best_pid = -1;
    int highest_priority = 99999;

    int i = readyQueue.front;
    while (i != readyQueue.rear) {
        i = (i + 1) % QUEUE_CAPACITY;
        int pid = readyQueue.items[i];

        if (PCB[pid].priority < highest_priority ||
            (PCB[pid].priority == highest_priority && PCB[pid].arrival_time < PCB[best_pid].arrival_time) ||
            (PCB[pid].priority == highest_priority && PCB[pid].arrival_time == PCB[best_pid].arrival_time && pid < best_pid)) {
            highest_priority = PCB[pid].priority;
            best_pid = pid;
        }
    }

    // 선택된 프로세스를 큐에서 제거
    if (best_pid != -1) {
        remove_from_queue(&readyQueue, best_pid);
    }

    return best_pid;
}


int Priority_Preemptive(int running_pid) {
    if (is_empty(&readyQueue)) {
        return running_pid;
    }

    int best_pid = -1;
    int highest_priority = 99999;

    int i = readyQueue.front;
    while (i != readyQueue.rear) {
        i = (i + 1) % QUEUE_CAPACITY;
        int pid = readyQueue.items[i];

        if (PCB[pid].priority < highest_priority ||
            (PCB[pid].priority == highest_priority && PCB[pid].arrival_time < PCB[best_pid].arrival_time) ||
            (PCB[pid].priority == highest_priority && PCB[pid].arrival_time == PCB[best_pid].arrival_time && pid < best_pid)) {
            highest_priority = PCB[pid].priority;
            best_pid = pid;
        }
    }

    // 선점
    if (best_pid != -1) {
        if (running_pid == -1 ||
            PCB[best_pid].priority < PCB[running_pid].priority ||
            (PCB[best_pid].priority == PCB[running_pid].priority && PCB[best_pid].arrival_time < PCB[running_pid].arrival_time) ||
            (PCB[best_pid].priority == PCB[running_pid].priority && PCB[best_pid].arrival_time == PCB[running_pid].arrival_time && best_pid < running_pid)) {
            if (running_pid != -1) {
                PCB[running_pid].state = READY;
                PCB[running_pid].time_slice_counter = 0;
                enqueue(&readyQueue, running_pid);
            }
            remove_from_queue(&readyQueue, best_pid);
            PCB[best_pid].time_slice_counter = 0;
            return best_pid;
        }
    }

    return running_pid;
}


int RR(int running_pid) {
    // 실행 중인 프로세스가 없으면 바로 선택
    if (running_pid == -1) {
        if (!is_empty(&readyQueue)) {
            int pid = dequeue(&readyQueue);
            PCB[pid].time_slice_counter = 0; // 카운터 초기화
            return pid;
        }
        return -1; // ReadyQueue가 비어있으면 IDLE
    }

    // 타임 슬라이스 카운터 증가
    PCB[running_pid].time_slice_counter++;

    // 타임 슬라이스 초과 시 선점
    if (PCB[running_pid].time_slice_counter >= TIME_QUANTUM) {
        PCB[running_pid].state = READY;
        PCB[running_pid].time_slice_counter = 0;
        enqueue(&readyQueue, running_pid);

        if (!is_empty(&readyQueue)) {
            int pid = dequeue(&readyQueue);
            PCB[pid].time_slice_counter = 0;
            return pid;
        }
        return -1;
    }

    // 타임 슬라이스 내에서 계속 실행
    return running_pid; 
}



int Lottery(int running_pid) {
    if (running_pid != -1) {
        return running_pid;
    }

    if (is_empty(&readyQueue)) {
        return -1;
    }

    int total_tickets = 0;

    int i = readyQueue.front;
    while (i != readyQueue.rear) {
        i = (i + 1) % QUEUE_CAPACITY;
        int pid = readyQueue.items[i];
        total_tickets += PCB[pid].ticket_count;
    }

    if (total_tickets == 0) return -1;

    int winning_ticket = rand() % total_tickets;

    int current_ticket = 0;
    int winner_pid = -1;

    i = readyQueue.front;
    while (i != readyQueue.rear) {
        i = (i + 1) % QUEUE_CAPACITY;
        int pid = readyQueue.items[i];
        current_ticket += PCB[pid].ticket_count;
        if (winning_ticket < current_ticket) {
            winner_pid = pid;
            break;
        }
    }

    if (winner_pid != -1) {
        remove_from_queue(&readyQueue, winner_pid);
    }

    return winner_pid;
}


int Schedule(int running_pid, int mode) {
    switch (mode) {
    case FCFS_MODE:
        return FCFS(running_pid);
    case SJF_MODE:
        return SJF(running_pid);
    case SJF_PREEMPTIVE_MODE:
        return SJF_Preemptive(running_pid);
    case PRIORITY_MODE:
        return Priority(running_pid);
    case PRIORITY_PREEMPTIVE_MODE:
        return Priority_Preemptive(running_pid);
    case RR_MODE:
        return RR(running_pid);
    case LOTTERY_MODE:
        return Lottery(running_pid);
    default:
        return running_pid;
    }
}

void Config(int mode) {
    int time = 0;
    int running_pid = -1;
    int all_terminated = 0;
    int finished_time = MAX_TIME;

    // Gantt chart 초기화
    for (int i = 0; i < MAX_TIME; i++) {
        gantt_chart[i] = -1;
    }

    while (time < MAX_TIME && !all_terminated) {
        // 1. 도착한 프로세스를 JobQueue에서 readyQueue로 이동
        jobqueue_to_readyqueue(time, mode);

        // 2. I/O가 끝난 프로세스들을 WAITING -> READY로 이동
        if (!is_empty(&waitingQueue)) {
            int temp_waiting[MAX_PROCESSES];
            int temp_count = 0;

            while (!is_empty(&waitingQueue)) {
                temp_waiting[temp_count++] = dequeue(&waitingQueue);
            }

            for (int i = 0; i < temp_count; i++) {
                int pid = temp_waiting[i];
                PCB[pid].io_remaining_time--;

                // I/O 완료 시
                if (PCB[pid].io_remaining_time <= 0) {
                    PCB[pid].state = READY;
                    PCB[pid].time_slice_counter = 0;

                    switch (mode) {
                        case PRIORITY_MODE:
                        case PRIORITY_PREEMPTIVE_MODE:
                            enqueue_priority(&readyQueue, pid);
                            break;
                        case SJF_MODE:
                        case SJF_PREEMPTIVE_MODE:
                            enqueue_sjf(&readyQueue, pid);
                            break;
                        default:
                            enqueue(&readyQueue, pid);
                            break;
                    }
                    printf("Time %2d: P%d finished I/O -> READY\n", time, pid);
                }
                else {
                    enqueue(&waitingQueue, pid);
                }
            }
        }

        // 3. 현재 실행 중인 프로세스가 있다면 CPU 실행
        if (running_pid != -1) {
            Process* p = &PCB[running_pid];

            if (p->start_time == -1) {
                p->start_time = time;
                p->response_time = time - p->arrival_time;
            }

            p->state = RUNNING;

            // I/O 요청 체크
            int cpu_used = p->burst_time - p->remaining_time + 1;
            int io_occurred = 0;

            // 실행 중일때만 I/O 요청 처리
            for (int j = 0; j < MAX_IO_REQUESTS && !io_occurred; j++) {
                if (!p->io_done[j] && cpu_used == p->io_request_times[j]) {
                    p->io_done[j] = 1;
                    p->state = WAITING;
                    p->io_remaining_time = p->io_burst_times[j];
                    p->time_slice_counter = 0;

                    int prev_pid = running_pid;
                    enqueue(&waitingQueue, running_pid);

                    printf("Time %2d: P%d (CPU used: %d) -> I/O %d (duration: %d)\n",
                        time, prev_pid, cpu_used, j + 1, p->io_burst_times[j]);

                    if (mode == RR_MODE) {
                        PCB[prev_pid].time_slice_counter = 0;
                    }

                    running_pid = -1;
                    io_occurred = 1;

                    break;
                }
            }


            // I/O 없으면 CPU 사용
            if (!io_occurred) {
                p->remaining_time--;
                if (p->remaining_time <= 0) {
                    p->state = TERMINATED;
                    p->finish_time = time;
                    printf("Time %2d: P%d -> TERMINATED\n", time, p->pid);
                    running_pid = -1;
                }
            }
        }

        // 4. 스케줄링 (선점형은 즉시 교체, 비선점형은 running_pid 없을 때만)
        if (mode == SJF_PREEMPTIVE_MODE || mode == PRIORITY_PREEMPTIVE_MODE) {
            running_pid = Schedule(running_pid, mode); // 선점형은 매 시간마다 확인
        }
        else if (running_pid == -1) {
            running_pid = Schedule(running_pid, mode); // 비선점형은 비어있을 때만
        }



        // 5. 간트차트
        if (running_pid != -1) {
            PCB[running_pid].state = RUNNING;
            if (PCB[running_pid].start_time == -1) {
                PCB[running_pid].start_time = time;
                PCB[running_pid].response_time = time - PCB[running_pid].arrival_time;
            }

            // 간트차트 기록
            gantt_chart[time] = running_pid;
        }
        else {
            gantt_chart[time] = -1; // IDLE
        }

        // 6. READY 상태 프로세스의 대기 시간 증가
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (PCB[i].state == READY) {
                PCB[i].waiting_time++;
            }
        }

        // 7. 모든 프로세스가 종료되었는지 확인
        all_terminated = 1;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (PCB[i].state != TERMINATED) {
                all_terminated = 0;
                break;
            }
        }

        // 모든 프로세스 종료 시 루프 탈출
        if (all_terminated) {
            finished_time = time; 
            break;
        }

        time++;
    }
}


void Evaluation(Process* p, int n) {
    double total_wait = 0;
    double total_turnaround = 0;
    int completed_processes = 0;

    printf("\n[Evaluation Results]\n");
    printf("PID\tArrival\tBurst\tFinish\tWaiting\tTurnaround\tStatus\n");

    for (int i = 0; i < n; i++) {
        if (p[i].state == TERMINATED && p[i].finish_time > 0) {
            p[i].turnaround_time = p[i].finish_time - p[i].arrival_time;
            total_wait += p[i].waiting_time;
            total_turnaround += p[i].turnaround_time;
            completed_processes++;
            printf("P%d\t%d\t%d\t%d\t%d\t%d\tCompleted\n",
                p[i].pid, p[i].arrival_time, p[i].burst_time,
                p[i].finish_time, p[i].waiting_time, p[i].turnaround_time);
        }
        else {
            printf("P%d\t%d\t%d\t-\t-\t-\tIncomplete\n",
                p[i].pid, p[i].arrival_time, p[i].burst_time);
        }
    }

    if (completed_processes > 0) {
        printf("\nCompleted Processes: %d/%d\n", completed_processes, n);
        printf("Average Waiting Time    : %.2lf\n", total_wait / completed_processes);
        printf("Average Turnaround Time : %.2lf\n", total_turnaround / completed_processes);
    }
    else {
        printf("\nNo processes completed!\n");
    }
}

void Print_GanttChart(int* gantt_chart, int end_time) {
    printf("\n[Gantt Chart]\n");
    int start = 0;
    int current = gantt_chart[0];

    for (int t = 1; t <= end_time; t++) {
        if (t == end_time || gantt_chart[t] != current) {
            if (current == -1)
                printf("%d ~ %d : IDLE\n", start, t); 
            else
                printf("%d ~ %d : P%d\n", start, t, current); 
            start = t;
            if (t < end_time) current = gantt_chart[t]; 
        }
    }
}

int main() {
    init_queue(&jobQueue);
    init_queue(&readyQueue);
    init_queue(&waitingQueue);

    Create_Processes();
    clone_state();

    const char* alg_names[] = {
        "FCFS",
        "SJF (Non-Preemptive)",
        "SJF (Preemptive)",
        "Priority (Non-Preemptive)",
        "Priority (Preemptive)",
        "Round Robin",
        "Lottery"
    };

    const int alg_modes[] = {
        FCFS_MODE,
        SJF_MODE,
        SJF_PREEMPTIVE_MODE,
        PRIORITY_MODE,
        PRIORITY_PREEMPTIVE_MODE,
        RR_MODE,
        LOTTERY_MODE
    };

    int total_algorithms = sizeof(alg_modes) / sizeof(int);

    // 모든 알고리즘 실행
    for (int i = 0; i < total_algorithms; i++) {
        printf("\n\n============================================\n");
        printf("Simulation: %s\n", alg_names[i]);
        printf("============================================\n");

        // 이전 상태 복원
        load_clone_state();

        // readyQueue, waitingQueue도 초기화
        init_queue(&readyQueue);
        init_queue(&waitingQueue);

        // 시뮬레이션 실행
        Config(alg_modes[i]);

        // 평가 출력
        Evaluation(PCB, MAX_PROCESSES);

        // 간트 차트 출력
        Print_GanttChart(gantt_chart, MAX_TIME);
    }

    return 0;
}