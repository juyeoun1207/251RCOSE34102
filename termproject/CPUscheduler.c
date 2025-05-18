#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_PROCESSES 5
#define MAX_TIME 100
#define MAX_IO_REQUESTS 3

#define TIME_QUANTUM 3

#define FCFS_MODE 0
#define SJF_MODE 1
#define SJF_PREEMPTIVE_MODE 2
#define PRIORITY_MODE 3
#define PRIORITY_PREEMPTIVE_MODE 4
#define RR_MODE 5


int gantt_chart[MAX_TIME];


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

    State state;
} Process;

// ready, waiting 용도의 큐 구조
typedef struct {
    int items[MAX_PROCESSES];
    int front;
    int rear;
} Queue;

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

void enqueue(Queue* q, int value) {
    if (q->rear == MAX_PROCESSES - 1) return;
    if (in_queue(q, value)) return; 
    if (q->front == -1) q->front = 0;
    q->items[++(q->rear)] = value;
}


int dequeue(Queue* q) {
    if (is_empty(q)) return -1;
    int val = q->items[q->front];
    if (q->front == q->rear) q->front = q->rear = -1;
    else q->front++;
    return val;
}

// 랜덤하게 프로세스 생성
void Create_Process(Process* p, int n) {
    srand(time(NULL));
    for (int i = 0; i < n; i++) {
        p[i].pid = i + 1;
        p[i].arrival_time = rand() % 6; // 0~5 사이 도착
        p[i].burst_time = rand() % 7 + 6; // 6~12 사이 CPU 시간
        p[i].remaining_time = p[i].burst_time;

        for (int j = 0; j < MAX_IO_REQUESTS; j++) {
            // 각 I/O 요청 시점은 1~(burst_time-2)
            p[i].io_request_times[j] = rand() % (p[i].burst_time - 2) + 1;
            p[i].io_burst_times[j] = rand() % 3 + 2; // I/O는 2~4 걸림
            p[i].io_done[j] = 0; //아직 수행X
        }

        p[i].io_remaining_time = 0;
        p[i].priority = rand() % 5 + 1;
        p[i].start_time = -1;
        p[i].finish_time = 0;
        p[i].waiting_time = 0;
        p[i].turnaround_time = 0;
        p[i].response_time = -1;
        p[i].state = NEW;
    }
}


int FCFS(Process* p, Queue* ready, int running_pid) {
    if (running_pid != -1) {
        return running_pid;
    }

    if (!is_empty(ready)) {
        return dequeue(ready);
    }
    return -1;
}


int SJF(Process* p, Queue* ready, int running_pid) {
    if (running_pid != -1) {
        return running_pid;
    }

    if (is_empty(ready)) return -1;

    int shortest_index = -1;
    int min_time = 1e9;

    for (int i = ready->front; i <= ready->rear; i++) {
        int idx = ready->items[i];
        if (p[idx].remaining_time < min_time) {
            min_time = p[idx].remaining_time;
            shortest_index = i;
        }
    }

    if (shortest_index != -1) {
        int selected = ready->items[shortest_index];
        for (int i = shortest_index; i < ready->rear; i++)
            ready->items[i] = ready->items[i + 1];
        ready->rear--;
        if (ready->rear < ready->front) ready->front = ready->rear = -1;
        return selected;
    }
    return -1;
}


int SJF_preemptive(Process* p, Queue* ready, int running_pid) {
    int shortest_index = -1;
    int min_time = 1e9;

    for (int i = ready->front; i <= ready->rear; i++) {
        int idx = ready->items[i];
        if (p[idx].remaining_time < min_time) {
            min_time = p[idx].remaining_time;
            shortest_index = i;
        }
    }

    if (shortest_index != -1) {
        int selected = ready->items[shortest_index];

        if (running_pid == -1 || p[selected].remaining_time < p[running_pid].remaining_time) {
            if (running_pid != -1) enqueue(ready, running_pid);

            for (int i = shortest_index; i < ready->rear; i++)
                ready->items[i] = ready->items[i + 1];
            ready->rear--;
            if (ready->rear < ready->front) ready->front = ready->rear = -1;

            return selected;
        }
    }
    return running_pid;
}


int Priority(Process* p, Queue* ready, int running_pid) {
    if (running_pid != -1) return running_pid;

    if (is_empty(ready)) return -1;

    int best_idx = -1;
    int highest_priority = 1e9;

    for (int i = ready->front; i <= ready->rear; i++) {
        int idx = ready->items[i];
        if (p[idx].priority < highest_priority) {
            highest_priority = p[idx].priority;
            best_idx = i;
        }
    }

    if (best_idx != -1) {
        int selected = ready->items[best_idx];
        for (int i = best_idx; i < ready->rear; i++)
            ready->items[i] = ready->items[i + 1];
        ready->rear--;
        if (ready->rear < ready->front) ready->front = ready->rear = -1;
        return selected;
    }
    return -1;
}


int Priority_preemptive(Process* p, Queue* ready, int running_pid) {
    if (is_empty(ready)) return running_pid;

    int best_idx = -1;
    int highest_priority = 1e9;

    for (int i = ready->front; i <= ready->rear; i++) {
        int idx = ready->items[i];
        if (p[idx].priority < highest_priority) {
            highest_priority = p[idx].priority;
            best_idx = i;
        }
    }
    if (best_idx != -1) {
        int selected = ready->items[best_idx];

        if (running_pid == -1 || p[selected].priority < p[running_pid].priority) {
            if (running_pid != -1) enqueue(ready, running_pid);

            for (int i = best_idx; i < ready->rear; i++)
                ready->items[i] = ready->items[i + 1];
            ready->rear--;
            if (ready->rear < ready->front) ready->front = ready->rear = -1;

            return selected;
        }
    }
    return running_pid;
}


int RR(Queue* ready, int running_pid, int* time_slice_counter, int time_quantum) {
    if (running_pid == -1 && !is_empty(ready)) {
        *time_slice_counter = 0;
        return dequeue(ready);
    }

    if (running_pid != -1) {
        (*time_slice_counter)++;

        if (*time_slice_counter >= time_quantum) {
            enqueue(ready, running_pid); // 다시 큐 뒤로
            *time_slice_counter = 0;
            return is_empty(ready) ? -1 : dequeue(ready);
        }
    }
    return running_pid;
}


int Schedule(Process* p, Queue* ready, int running_pid, int mode, int* time_slice_counter) {
    switch (mode) {
    case FCFS_MODE: return FCFS(p, ready, running_pid);
    case SJF_MODE: return SJF(p, ready, running_pid);
    case SJF_PREEMPTIVE_MODE: return SJF_preemptive(p, ready, running_pid);
    case PRIORITY_MODE: return Priority(p, ready, running_pid);
    case PRIORITY_PREEMPTIVE_MODE: return Priority_preemptive(p, ready, running_pid);
    case RR_MODE: return RR(ready, running_pid, time_slice_counter, TIME_QUANTUM);
    default: return running_pid;
    }
}


void Config(Process* p, int n, int* gantt_chart, int mode) {
    Queue ready;
    init_queue(&ready);

    int time = 0;
    int running_pid = -1;
    int time_slice_counter = 0;

    while (time < MAX_TIME) {
        // 1. 도착한 프로세스를 READY로 이동
        for (int i = 0; i < n; i++) {
            if (p[i].arrival_time == time && p[i].state == NEW) {
                p[i].state = READY;
                enqueue(&ready, i);
                printf("Time %2d: P%d arrived → READY\n", time, p[i].pid);
            }
        }

        // 2. WAITING 상태 프로세스 I/O 처리
        for (int i = 0; i < n; i++) {
            if (p[i].state == WAITING) {
                p[i].io_remaining_time--;
                if (p[i].io_remaining_time <= 0) {
                    p[i].state = READY;
                    enqueue(&ready, i);
                    printf("Time %2d: P%d finished I/O → READY\n", time, p[i].pid);
                }
            }
        }

        // 3. RUNNING 상태에서 I/O 요청이 발생하면 WAITING으로 전이
        if (running_pid != -1) {
            int idx = running_pid;
            int exec_time = p[idx].burst_time - p[idx].remaining_time;

            for (int j = 0; j < MAX_IO_REQUESTS; j++) {
                if (!p[idx].io_done[j] && exec_time == p[idx].io_request_times[j]) {
                    p[idx].state = WAITING;
                    p[idx].io_remaining_time = p[idx].io_burst_times[j];
                    p[idx].io_done[j] = 1;
                    printf("Time %2d: P%d → WAITING (I/O %d start)\n", time, p[idx].pid, j + 1);
                    running_pid = -1; // CPU 비움
                    break;
                }
            }
        }

        // 4. 스케줄러 호출 (선점형 포함)
        int prev_pid = running_pid;
        running_pid = Schedule(p, &ready, running_pid, mode, &time_slice_counter);

        // 5. 선점 발생 시 READY로 복귀
        if (prev_pid != -1 && prev_pid != running_pid &&
            p[prev_pid].state == RUNNING && mode != RR_MODE) {
            p[prev_pid].state = READY;
            enqueue(&ready, prev_pid);
            printf("Time %2d: P%d preempted → READY\n", time, p[prev_pid].pid);
        }

        // 6. 실행 프로세스 처리
        if (running_pid != -1 && p[running_pid].state != WAITING && p[running_pid].state != TERMINATED) {
            Process* r = &p[running_pid];

            if (r->start_time == -1) r->start_time = time;
            if (r->response_time == -1) r->response_time = time - r->arrival_time;

            r->state = RUNNING;
            r->remaining_time--;

            if (r->remaining_time == 0) {
                r->state = TERMINATED;
                r->finish_time = time + 1;
                running_pid = -1;
                printf("Time %2d: P%d → TERMINATED\n", time + 1, r->pid);
            }
        }

        // 7. Gantt 차트 기록
        if (running_pid != -1 && p[running_pid].state == RUNNING)
            gantt_chart[time] = p[running_pid].pid;
        else
            gantt_chart[time] = 0;

        // 8. READY 상태 프로세스의 대기 시간 증가
        for (int i = 0; i < n; i++) {
            if (p[i].state == READY) {
                p[i].waiting_time++;
            }
        }

        time++;
    }
}


void Evaluation(Process* p, int n) {
    double total_wait = 0, total_turnaround = 0;
    printf("\n[Evaluation Results]\n");
    printf("PID\tArrival\tBurst\tFinish\tWaiting\tTurnaround\n");
    for (int i = 0; i < n; i++) {
        p[i].turnaround_time = p[i].finish_time - p[i].arrival_time;
        total_wait += p[i].waiting_time;
        total_turnaround += p[i].turnaround_time;
        printf("P%d\t%d\t\t%d\t%d\t%d\t%d\n", p[i].pid, p[i].arrival_time,
            p[i].burst_time, p[i].finish_time,
            p[i].waiting_time, p[i].turnaround_time);
    }

    printf("\nAverage Waiting Time    : %.2lf\n", total_wait / n);
    printf("Average Turnaround Time : %.2lf\n", total_turnaround / n);
}


void Print_GanttChart(int* gantt_chart, int max_time) {
    printf("\n[Gantt Chart]\n");

    // 상단 프로세스 구간
    printf("Time : ");
    for (int t = 0; t < max_time; t++) {
        if (gantt_chart[t] == 0)
            printf(" | -");
        else
            printf(" | P%d", gantt_chart[t]);
    }
    printf(" |\n");

    // 시간 눈금
    printf("       ");
    for (int t = 0; t <= max_time; t++) {
        printf("%2d ", t);
    }
    printf("\n");
}


int main() {
    Process p[MAX_PROCESSES];
    int gantt_chart[MAX_TIME];
    int mode = PRIORITY_PREEMPTIVE_MODE; // 바꿔가며 테스트 가능

    Create_Process(p, MAX_PROCESSES);
    Config(p, MAX_PROCESSES, gantt_chart, mode);
    Evaluation(p, MAX_PROCESSES);
    Print_GanttChart(gantt_chart, MAX_TIME);

    return 0;
}