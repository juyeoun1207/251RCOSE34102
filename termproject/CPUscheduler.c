#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_PROCESSES 5
#define MAX_TIME 50
#define MAX_IO_REQUESTS 3
#define TIME_QUANTUM 3

#define FCFS_MODE 0
#define SJF_MODE 1
#define SJF_PREEMPTIVE_MODE 2
#define PRIORITY_MODE 3
#define PRIORITY_PREEMPTIVE_MODE 4
#define RR_MODE 5


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
    int io_done[MAX_IO_REQUESTS]; //각 I/O 요청이 수행되었는지 여부 (0-X, 1-O)

    int priority;
    int start_time;
    int finish_time;
    int waiting_time;
    int turnaround_time;
    int response_time;

    int time_slice_counter;
    State state;
} Process;


// ready, waiting 용도의 큐 구조
typedef struct {
    int items[MAX_PROCESSES];
    int front;
    int rear;
} Queue;


Process PCB[MAX_PROCESSES];
Queue jobQueue;
Queue readyQueue;
Queue waitingQueue;
int gantt_chart[MAX_TIME];


// 클론 만들기
Process PCB_clone[MAX_PROCESSES];
Queue jobQueue_clone;


// 큐 초기화
void init_queue(Queue* q) {
    q->front = q->rear = -1;
}


int is_empty(Queue* q) {
    return q->front == -1;
}


int in_queue(Queue* q, int value) {
    for (int i = q->front; i <= q->rear; i++) {
        if (q->items[i] == value) return 1;
    }
    return 0;
}


void enqueue(Queue* q, int pid) {
    if (q->rear == MAX_PROCESSES - 1) return;
    if (in_queue(q, pid)) return;
    if (q->front == -1) q->front = 0;
    q->items[++(q->rear)] = pid;
}


// 우선순위를 고려하여 큐에 삽입
void enqueue_priority(Queue* q, int value) {
    if (q->rear == MAX_PROCESSES - 1) return;
    if (in_queue(q, value)) return;

    if (is_empty(q)) {
        q->front = q->rear = 0;
        q->items[0] = value;
        return;
    }

    int i;
    for (i = q->front; i <= q->rear; i++) {
        if (PCB[q->items[i]].priority > PCB[value].priority) {
            for (int j = q->rear; j >= i; j--) {
                q->items[j + 1] = q->items[j];
            }
            q->items[i] = value;
            if (q->front == -1) q->front = 0;
            q->rear++;
            return;
        }
    }
    enqueue(q, value); // 우선순위가 더 낮거나 같은 경우 맨 뒤에 삽입
}


int dequeue(Queue* q) {
    if (is_empty(q)) return -1;
    int val = q->items[q->front];
    if (q->front == q->rear) q->front = q->rear = -1;
    else q->front++;
    return val;
}


// 큐에서 특정 프로세스 제거
int remove_from_queue(Queue* q, int pid) {
    if (is_empty(q)) return -1;

    int pos = -1;
    for (int i = q->front; i <= q->rear; i++) {
        if (q->items[i] == pid) {
            pos = i;
            break;
        }
    }

    if (pos == -1) return -1;

    // 해당 위치부터 앞으로 당기기
    for (int i = pos; i < q->rear; i++) {
        q->items[i] = q->items[i + 1];
    }
    q->rear--;

    if (q->rear < q->front) {
        q->front = q->rear = -1;
    }

    return pid;
}


// 클론 함수 (백업용)
void clone_state() {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB_clone[i] = PCB[i];
    }

    jobQueue_clone.front = jobQueue.front;
    jobQueue_clone.rear = jobQueue.rear;
    for (int i = jobQueue.front; i <= jobQueue.rear; i++) {
        jobQueue_clone.items[i] = jobQueue.items[i];
    }
}


// 클론 복원 (불러오기용)
void load_clone_state() {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB[i] = PCB_clone[i];
    }

    jobQueue.front = jobQueue_clone.front;
    jobQueue.rear = jobQueue_clone.rear;
    for (int i = jobQueue.front; i <= jobQueue.rear; i++) {
        jobQueue.items[i] = jobQueue_clone.items[i];
    }
}


// 시간에 따라 jobQueue에서 프로세스를 readyQueue로 이동
void jobqueue_to_readyqueue(int time) {
    int count = jobQueue.rear - jobQueue.front + 1;
    for (int i = 0; i < count; i++) {
        int pid = dequeue(&jobQueue);
        if (PCB[pid].arrival_time <= time && PCB[pid].state == NEW) {
            PCB[pid].state = READY;
            enqueue(&readyQueue, pid);
            printf("Time %2d: P%d arrived -> READY\n", time, PCB[pid].pid);
        }
        else {
            enqueue(&jobQueue, pid); // 아직 도착 안했으면 다시 큐에 넣음
        }
    }
}


// 랜덤한 프로세스 생성해서 jobQueue에 삽입
void Create_Processes() {
    srand(time(NULL));
    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB[i].pid = i;
        PCB[i].arrival_time = rand() % 6; // 0~5 도착
        PCB[i].burst_time = rand() % 7 + 6; // 6~12 CPU 시간
        PCB[i].remaining_time = PCB[i].burst_time;

        for (int j = 0; j < MAX_IO_REQUESTS; j++) {
            // 각 I/O 요청 시점은 1~(burst_time-2)
            PCB[i].io_request_times[j] = rand() % (PCB[i].burst_time - 2) + 1;
            PCB[i].io_burst_times[j] = rand() % 3 + 2; // 2~4 I/O 시간
            PCB[i].io_done[j] = 0; // 아직 수행X
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

        enqueue(&jobQueue, i);
    }
}



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

    for (int i = readyQueue.front; i <= readyQueue.rear; i++) {
        int pid = readyQueue.items[i];
        if (PCB[pid].remaining_time < min_time) {
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

    for (int i = readyQueue.front; i <= readyQueue.rear; i++) {
        int pid = readyQueue.items[i];
        if (PCB[pid].remaining_time < min_time) {
            min_time = PCB[pid].remaining_time;
            shortest_pid = pid;
        }
    }

    // 선점 조건 확인
    if (shortest_pid != -1) {
        if (running_pid == -1 || PCB[shortest_pid].remaining_time < PCB[running_pid].remaining_time) {
            // 선점 발생
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

    for (int i = readyQueue.front; i <= readyQueue.rear; i++) {
        int pid = readyQueue.items[i];
        if (PCB[pid].priority < highest_priority) {
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

    for (int i = readyQueue.front; i <= readyQueue.rear; i++) {
        int pid = readyQueue.items[i];
        if (PCB[pid].priority < highest_priority) {
            highest_priority = PCB[pid].priority;
            best_pid = pid;
        }
    }

    if (best_pid != -1) {
        if (running_pid == -1 || PCB[best_pid].priority < PCB[running_pid].priority) {
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


int Round_Robin(int running_pid) {
    if (running_pid != -1) {
        PCB[running_pid].time_slice_counter++;

        if (PCB[running_pid].time_slice_counter >= TIME_QUANTUM) {
            PCB[running_pid].state = READY;
            PCB[running_pid].time_slice_counter = 0;
            enqueue(&readyQueue, running_pid);
            running_pid = -1;
        }
    }

    // 새로운 프로세스 선택
    if (running_pid == -1) {
        int next_pid = dequeue(&readyQueue);
        if (next_pid != -1) {
            PCB[next_pid].time_slice_counter = 0;
        }
        return next_pid;
    }

    return running_pid;
}


int EDF(int running_pid) {
    if (running_pid != -1) {
        return running_pid;
    }

    if (is_empty(&readyQueue)) {
        return -1;
    }

    int earliest_pid = -1;
    int highest_priority = 99999;

    for (int i = readyQueue.front; i <= readyQueue.rear; i++) { //여기수정해야됨!!
        int pid = readyQueue.items[i];
        if (PCB[pid].priority < highest_priority) {
            highest_priority = PCB[pid].priority;
            best_pid = pid;
        }
    }

    // 선택된 프로세스를 큐에서 제거
    if (earliest_pid != -1) {
        remove_from_queue(&readyQueue, earliest_pid);
    }

    return best_pid;
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
        return Round_Robin(running_pid);
    default:
        return running_pid;
    }
}

void Config(int mode) {
    int time = 0;
    int running_pid = -1;

    while (time < MAX_TIME) {
        // 도착한 프로세스를 JobQueue에 저장해서 arrival time에 따라 readyQueue로 이동
        jobqueue_to_readyqueue(time);

        // I/O가 끝났을 때 WAITING -> READY
        int waiting_count = waitingQueue.rear - waitingQueue.front + 1;
        for (int i = 0; i < waiting_count; i++) {
            int pid = dequeue(&waitingQueue);
            if (--PCB[pid].io_remaining_time <= 0) {
                PCB[pid].state = READY;
                enqueue(&readyQueue, pid);
                printf("Time %2d: P%d finished I/O -> READY\n", time, pid);
            }
            else {
                enqueue(&waitingQueue, pid);
            }
        }

        // I/O 요청이 발생하면 RUNNING -> WAITING
        if (running_pid != -1) {
            Process* r = &PCB[running_pid];
            int exec_time = r->burst_time - r->remaining_time;
            for (int j = 0; j < MAX_IO_REQUESTS; j++) {
                if (!r->io_done[j] && exec_time == r->io_request_times[j]) {
                    r->io_done[j] = 1;
                    r->state = WAITING;
                    r->io_remaining_time = r->io_burst_times[j];
                    enqueue(&waitingQueue, running_pid);
                    printf("Time %2d: P%d -> WAITING (I/O %d start)\n", time, r->pid, j + 1);
                    running_pid = -1;
                    break;
                }
            }
        }

        // 스케줄러 호출
        running_pid = Schedule(running_pid, mode);

        // 실행 프로세스
        if (running_pid != -1 && PCB[running_pid].state != WAITING && PCB[running_pid].state != TERMINATED) {
            Process* r = &PCB[running_pid];

            if (r->start_time == -1) r->start_time = time;
            if (r->response_time == -1) r->response_time = time - r->arrival_time;

            r->state = RUNNING;
            r->remaining_time--;

            if (r->remaining_time == 0) {
                r->state = TERMINATED;
                r->finish_time = time + 1;
                printf("Time %2d: P%d -> TERMINATED\n", time + 1, r->pid);
                running_pid = -1;
            }
        }

        // Gantt 차트 기록
        if (running_pid != -1 && PCB[running_pid].state == RUNNING)
            gantt_chart[time] = PCB[running_pid].pid;
        else
            gantt_chart[time] = -1;

        // READY 상태 프로세스의 대기 시간 증가
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (PCB[i].state == READY) {
                PCB[i].waiting_time++;
            }
        }

        time++;
    }
}

void Evaluation(Process* p, int n) {
    double total_wait = 0;
    double total_turnaround = 0;
    printf("\n[Evaluation Results]\n");
    printf("PID\tArrival\tBurst\tFinish\tWaiting\tTurnaround\n");
    for (int i = 0; i < n; i++) {
        p[i].turnaround_time = p[i].finish_time - p[i].arrival_time;
        total_wait += p[i].waiting_time;
        total_turnaround += p[i].turnaround_time;
        printf("P%d\t%d\t%d\t%d\t%d\t%d\n", p[i].pid, p[i].arrival_time, p[i].burst_time, p[i].finish_time, p[i].waiting_time, p[i].turnaround_time);
    }

    printf("\nAverage Waiting Time    : %.2lf\n", total_wait / n);
    printf("Average Turnaround Time : %.2lf\n", total_turnaround / n);
}

void Print_GanttChart(int* gantt_chart, int max_time) {
    printf("\n[Gantt Chart]\n");

    int start = 0;
    int current = gantt_chart[0];

    for (int t = 1; t <= max_time; t++) {
        if (t == max_time || gantt_chart[t] != current) {
            if (current == -1)
                printf("%d ~ %d : IDLE\n", start, t);
            else
                printf("%d ~ %d : P%d\n", start, t, current);

            start = t;
            if (t < max_time) current = gantt_chart[t];
        }
    }
}

int main() {
    init_queue(&jobQueue);
    init_queue(&readyQueue);
    init_queue(&waitingQueue);

    Create_Processes();
    clone_state(); // PCB[], jobQueue 복사

    // 알고리즘 목록
    const char* alg_names[] = {
        "FCFS",
        "SJF (Non-Preemptive)",
        "SJF (Preemptive)",
        "Priority (Non-Preemptive)",
        "Priority (Preemptive)",
        "Round Robin"
    };

    const int alg_modes[] = {
        FCFS_MODE,
        SJF_MODE,
        SJF_PREEMPTIVE_MODE,
        PRIORITY_MODE,
        PRIORITY_PREEMPTIVE_MODE,
        RR_MODE
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