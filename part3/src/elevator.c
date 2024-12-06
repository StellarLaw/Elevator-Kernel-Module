#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group 30");
MODULE_DESCRIPTION("Elevator Kernel Module");

// External declarations for syscall stubs
extern int (*STUB_start_elevator)(void);
extern int (*STUB_issue_request)(int, int, int);
extern int (*STUB_stop_elevator)(void);

#define ENTRY_NAME "elevator"
#define PERMS 0644
#define PARENT NULL
#define MAX_FLOORS 6
#define MAX_WEIGHT 750
#define FRESHMAN_WEIGHT 100
#define SOPHOMORE_WEIGHT 150
#define JUNIOR_WEIGHT 200
#define SENIOR_WEIGHT 250

// States enum
enum elevator_state {
    OFFLINE,
    IDLE,
    LOADING,
    UP,
    DOWN
};

// Passenger types enum
enum passenger_type {
    FRESHMAN = 'F',
    SOPHOMORE = 'O',
    JUNIOR = 'J',
    SENIOR = 'S'
};

// Structures
struct passenger {
    enum passenger_type type;
    int start_floor;
    int dest_floor;
    struct list_head list;
};

// Floor structure
struct floor {
    struct list_head passengers;
    int waiting_count;
};

// Elevator structure
struct elevator {
    enum elevator_state state;
    int current_floor;
    int current_weight;
    struct list_head passengers;
    int passenger_count;
    int total_serviced;
    bool running;
};

// Global variables
static struct proc_dir_entry* elevator_entry;
static struct elevator* elevator;
static struct floor* floors;
static struct task_struct* elevator_thread;
static DEFINE_MUTEX(elevator_mutex);

// Helper Functions
static const char* get_state_string(enum elevator_state state) {
    switch (state) {
        case OFFLINE: return "OFFLINE";
        case IDLE: return "IDLE";
        case LOADING: return "LOADING";
        case UP: return "UP";
        case DOWN: return "DOWN";
        default: return "UNKNOWN";
    }
}

static int get_passenger_weight(enum passenger_type type) {
    switch(type) {
        case FRESHMAN: return FRESHMAN_WEIGHT;
        case SOPHOMORE: return SOPHOMORE_WEIGHT;
        case JUNIOR: return JUNIOR_WEIGHT;
        case SENIOR: return SENIOR_WEIGHT;
        default: return 0;
    }
}

// Core elevator functions
static int start_elevator(void) {
    mutex_lock(&elevator_mutex);
    
    if (!elevator) {
        mutex_unlock(&elevator_mutex);
        return -ENOMEM;  // Memory allocation failed
    }
    
    if (elevator->state != OFFLINE) {
        mutex_unlock(&elevator_mutex);
        return 1;  // Return 1 if elevator is already active
    }
    
    // Initialize elevator
    elevator->running = true;
    elevator->state = IDLE;
    elevator->current_floor = 1;
    elevator->passenger_count = 0;
    // Don't reset total_serviced
    
    wake_up_process(elevator_thread);
    mutex_unlock(&elevator_mutex);
    return 0;  // Successful start
}

static int stop_elevator(void) {
    mutex_lock(&elevator_mutex);
    
    if (!elevator) {
        mutex_unlock(&elevator_mutex);
        return -ENOMEM;
    }
    
    if (elevator->state == OFFLINE) {
        mutex_unlock(&elevator_mutex);
        return 0;  // Already offline
    }
    
    // Check if there are any passengers in elevator
    if (elevator->passenger_count > 0) {
        mutex_unlock(&elevator_mutex);
        return 1;  // Return 1 if elevator is in process of deactivating
    }
    
    elevator->running = false;
    elevator->state = OFFLINE;
    mutex_unlock(&elevator_mutex);
    return 0;  // Successfully stopped
}




static int add_passenger(int type, int start_floor, int dest_floor) {
    struct passenger* p;
    enum passenger_type p_type;

    if (start_floor < 1 || start_floor > MAX_FLOORS ||
        dest_floor < 1 || dest_floor > MAX_FLOORS ||
        start_floor == dest_floor) {
        return -EINVAL;
    }

    switch(type) {
        case 0: p_type = FRESHMAN; break;
        case 1: p_type = SOPHOMORE; break;
        case 2: p_type = JUNIOR; break;
        case 3: p_type = SENIOR; break;
        default: return -EINVAL;
    }

    p = kmalloc(sizeof(*p), GFP_KERNEL);
    if (!p) return -ENOMEM;

    p->type = p_type;
    p->start_floor = start_floor;
    p->dest_floor = dest_floor;
    INIT_LIST_HEAD(&p->list);

    mutex_lock(&elevator_mutex);
    list_add_tail(&p->list, &floors[start_floor-1].passengers);
    floors[start_floor-1].waiting_count++;
    mutex_unlock(&elevator_mutex);

    return 0;
}

// Thread function
static int elevator_run(void* data) {
    struct passenger *p, *temp;
    int i;

    while (!kthread_should_stop()) {
        mutex_lock(&elevator_mutex);

        if (!elevator->running || elevator->state == OFFLINE) {
            mutex_unlock(&elevator_mutex);
            msleep(100);
            continue;
        }

        switch(elevator->state) {
            case IDLE: {
                bool need_up = false;
                bool need_down = false;
                bool should_load = false;

                // First priority: Check for unloading at current floor
                list_for_each_entry(p, &elevator->passengers, list) {
                    if (p->dest_floor == elevator->current_floor) {
                        should_load = true;
                        break;
                    }
                }

                // Second priority: Check if we can load at current floor
                if (!should_load && elevator->passenger_count < 5 && 
                    !list_empty(&floors[elevator->current_floor-1].passengers)) {
                    
                    struct passenger *first_waiting = list_first_entry_or_null(
                        &floors[elevator->current_floor-1].passengers,
                        struct passenger,
                        list
                    );
                    
                    if (first_waiting && 
                        elevator->current_weight + get_passenger_weight(first_waiting->type) <= MAX_WEIGHT) {
                        should_load = true;
                    }
                }

                if (should_load) {
                    elevator->state = LOADING;
                    break;
                }

                // Third priority: Determine direction based on existing passengers
                list_for_each_entry(p, &elevator->passengers, list) {
                    if (p->dest_floor > elevator->current_floor) {
                        need_up = true;
                        break;
                    } else if (p->dest_floor < elevator->current_floor) {
                        need_down = true;
                        break;
                    }
                }

                // Fourth priority: Check for waiting passengers on other floors
                if (!need_up && !need_down) {
                    // First check floors above
                    for (i = elevator->current_floor + 1; i <= MAX_FLOORS; i++) {
                        if (floors[i-1].waiting_count > 0) {
                            need_up = true;
                            break;
                        }
                    }
                    
                    // Then check floors below if we're not going up
                    if (!need_up) {
                        for (i = elevator->current_floor - 1; i >= 1; i--) {
                            if (floors[i-1].waiting_count > 0) {
                                need_down = true;
                                break;
                            }
                        }
                    }
                }

                if (need_up) {
                    elevator->state = UP;
                } else if (need_down) {
                    elevator->state = DOWN;
                }
                break;
            }

            case LOADING: {
                bool made_changes = false;

                // First unload all passengers at current floor
                list_for_each_entry_safe(p, temp, &elevator->passengers, list) {
                    if (p->dest_floor == elevator->current_floor) {
                        int passenger_weight = get_passenger_weight(p->type);
                        list_del(&p->list);
                        elevator->current_weight -= passenger_weight;
                        elevator->passenger_count--;
                        elevator->total_serviced++;
                        kfree(p);
                        made_changes = true;
                    }
                }

                // Then try loading new passengers
                while (elevator->passenger_count < 5 && 
                       !list_empty(&floors[elevator->current_floor-1].passengers)) {
                    
                    struct passenger *next_passenger = list_first_entry_or_null(
                        &floors[elevator->current_floor-1].passengers,
                        struct passenger,
                        list
                    );

                    if (!next_passenger) break;

                    int new_weight = get_passenger_weight(next_passenger->type);
                    if (elevator->current_weight + new_weight > MAX_WEIGHT) {
                        break;  // Can't load any more passengers due to weight
                    }

                    list_del(&next_passenger->list);
                    list_add_tail(&next_passenger->list, &elevator->passengers);
                    elevator->current_weight += new_weight;
                    elevator->passenger_count++;
                    floors[elevator->current_floor-1].waiting_count--;
                    made_changes = true;
                }

                if (made_changes) {
                    mutex_unlock(&elevator_mutex);
                    msleep(1000);  // Loading/unloading time
                    mutex_lock(&elevator_mutex);
                }

                elevator->state = IDLE;  // Always return to IDLE to reassess situation
                break;
            }

            case UP: {
                if (elevator->current_floor < MAX_FLOORS) {
                    mutex_unlock(&elevator_mutex);
                    msleep(2000);  // Moving time
                    mutex_lock(&elevator_mutex);
                    
                    elevator->current_floor++;
                    elevator->state = IDLE;  // Reassess at new floor
                } else {
                    elevator->state = IDLE;
                }
                break;
            }

            case DOWN: {
                if (elevator->current_floor > 1) {
                    mutex_unlock(&elevator_mutex);
                    msleep(2000);  // Moving time
                    mutex_lock(&elevator_mutex);
                    
                    elevator->current_floor--;
                    elevator->state = IDLE;  // Reassess at new floor
                } else {
                    elevator->state = IDLE;
                }
                break;
            }

            default:
                elevator->state = IDLE;
                break;
        }

        mutex_unlock(&elevator_mutex);
        msleep(100);  // Small delay between iterations
    }

    return 0;
}


// Proc file operations
static ssize_t elevator_read(struct file* file, char __user* ubuf, size_t count, loff_t* ppos) {
    char* buf;
    int len = 0;
    ssize_t ret;
    struct passenger* p;
    int i, total_waiting = 0;

    if (*ppos > 0) 
        return 0;

    buf = kmalloc(4096, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    mutex_lock(&elevator_mutex);

    // Basic status
    len += scnprintf(buf + len, 4096 - len, 
        "Elevator state: %s\n"
        "Current floor: %d\n"
        "Current load: %d lbs\n\n"
        "Elevator status:",
        get_state_string(elevator->state),
        elevator->current_floor,
        elevator->current_weight
    );

    // Print passengers in elevator with better spacing
    list_for_each_entry(p, &elevator->passengers, list) {
        len += scnprintf(buf + len, 4096 - len, " %c%d", p->type, p->dest_floor);
    }
    len += scnprintf(buf + len, 4096 - len, "\n\n");

    // Floor status
    for (i = MAX_FLOORS; i >= 1; i--) {
        len += scnprintf(buf + len, 4096 - len, "[%c] Floor %d: %2d", 
            (elevator->current_floor == i) ? '*' : ' ',
            i, 
            floors[i-1].waiting_count
        );
        
        list_for_each_entry(p, &floors[i-1].passengers, list) {
            len += scnprintf(buf + len, 4096 - len, " %c%d", p->type, p->dest_floor);
        }
        len += scnprintf(buf + len, 4096 - len, "\n");
        total_waiting += floors[i-1].waiting_count;
    }

    // Summary statistics
    len += scnprintf(buf + len, 4096 - len, "\n"
        "Number of passengers: %d\n"
        "Number of passengers waiting: %d\n"
        "Number of passengers serviced: %d\n",
        elevator->passenger_count,
        total_waiting,
        elevator->total_serviced
    );

    mutex_unlock(&elevator_mutex);

    ret = simple_read_from_buffer(ubuf, count, ppos, buf, len);
    kfree(buf);
    return ret;
}

static ssize_t elevator_write(struct file* file, const char __user* ubuf, size_t count, loff_t* ppos) {
    char buf[128];
    int ret = 0;
    size_t buf_size = min(count, sizeof(buf)-1);

    if (copy_from_user(buf, ubuf, buf_size))
        return -EFAULT;

    buf[buf_size] = '\0';

    if (strncmp(buf, "start", 5) == 0) {
        ret = start_elevator();
    } else if (strncmp(buf, "stop", 4) == 0) {
        ret = stop_elevator();
    } else {
        char type;
        int start_floor, dest_floor;
        int p_type;

        if (sscanf(buf, "%c %d %d", &type, &start_floor, &dest_floor) != 3)
            return -EINVAL;

        switch(type) {
            case 'f': case 'F': p_type = 0; break;
            case 'o': case 'O': p_type = 1; break;
            case 'j': case 'J': p_type = 2; break;
            case 's': case 'S': p_type = 3; break;
            default: return -EINVAL;
        }

        ret = add_passenger(p_type, start_floor, dest_floor);
    }

    if (ret)
        return ret;
    return count;
}

static int elevator_issue_request(int start_floor, int dest_floor, int type) {
    // Validate parameters
    if (start_floor < 1 || start_floor > MAX_FLOORS ||
        dest_floor < 1 || dest_floor > MAX_FLOORS ||
        start_floor == dest_floor ||
        type < 0 || type > 3) {
        return 1;  // Return 1 for invalid request
    }

    if (add_passenger(type, start_floor, dest_floor) != 0) {
        return 1;  // Return 1 if add_passenger failed
    }

    return 0;  // Return 0 for successful request
}


static const struct proc_ops elevator_fops = {
    .proc_read = elevator_read,
    .proc_write = elevator_write,
};


// Module init
static int __init elevator_init(void) {
    int i;

    elevator_entry = proc_create(ENTRY_NAME, PERMS, PARENT, &elevator_fops);
    if (!elevator_entry)
        return -ENOMEM;

    elevator = kmalloc(sizeof(*elevator), GFP_KERNEL);
    if (!elevator) {
        proc_remove(elevator_entry);
        return -ENOMEM;
    }

    floors = kmalloc(sizeof(struct floor) * MAX_FLOORS, GFP_KERNEL);
    if (!floors) {
        proc_remove(elevator_entry);
        kfree(elevator);
        return -ENOMEM;
    }

    // Initialize elevator
    elevator->state = OFFLINE;
    elevator->current_floor = 1;
    elevator->current_weight = 0;
    elevator->passenger_count = 0;
    elevator->total_serviced = 0;
    elevator->running = false;
    INIT_LIST_HEAD(&elevator->passengers);

    // Initialize floors
    for (i = 0; i < MAX_FLOORS; i++) {
        INIT_LIST_HEAD(&floors[i].passengers);
        floors[i].waiting_count = 0;
    }

    // Create elevator thread
    elevator_thread = kthread_run(elevator_run, NULL, "elevator_thread");
    if (IS_ERR(elevator_thread)) {
        kfree(floors);
        kfree(elevator);
        proc_remove(elevator_entry);
        return PTR_ERR(elevator_thread);
    }

    // Connect syscall stubs
    STUB_start_elevator = start_elevator;
    STUB_issue_request = elevator_issue_request;
    STUB_stop_elevator = stop_elevator;

    printk(KERN_INFO "Elevator module initialized\n");
    return 0;
}

// Module cleanup
static void __exit elevator_exit(void) {
    int i;
    struct passenger *p, *temp;

    // Disconnect syscall stubs
    STUB_start_elevator = NULL;
    STUB_issue_request = NULL;
    STUB_stop_elevator = NULL;

    kthread_stop(elevator_thread);

    // Free elevator passengers
    list_for_each_entry_safe(p, temp, &elevator->passengers, list) {
        list_del(&p->list);
        kfree(p);
    }

    // Free waiting passengers
    for (i = 0; i < MAX_FLOORS; i++) {
        list_for_each_entry_safe(p, temp, &floors[i].passengers, list) {
            list_del(&p->list);
            kfree(p);
        }
    }

    kfree(floors);
    kfree(elevator);
    proc_remove(elevator_entry);
    printk(KERN_INFO "Elevator module removed\n");
}

module_init(elevator_init);
module_exit(elevator_exit);
