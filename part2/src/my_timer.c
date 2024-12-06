#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/timekeeping.h>

// MODULE_LICENSE is required; author and description is nice to have
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group 30");
MODULE_DESCRIPTION("Timer Kernel Module");

// Define constants for /proc entry
#define ENTRY_NAME "timer"
#define PERMS 0644
#define PARENT NULL

static struct proc_dir_entry* timer_entry;
static struct timespec64 last_time;
static bool first_read;

// Read from /proc/timer
static ssize_t timer_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
    struct timespec64 current_time, diff;
    char buf[256];
    int len = 0;

    // If /proc/timer is read already, quit. Prevents elapsed time from showing
    // in the first read
    if (*ppos > 0)
    {
	return 0;
    }

    // Initialize current time
    ktime_get_real_ts64(&current_time);

    if (!first_read) 
    {
        // Calculate the elapsed time since the last read
        diff.tv_sec = current_time.tv_sec - last_time.tv_sec;
        diff.tv_nsec = current_time.tv_nsec - last_time.tv_nsec;

        // Adjust for negative nanoseconds by borrowing from seconds
        if (diff.tv_nsec < 0)
       	{
            diff.tv_sec -= 1;
            diff.tv_nsec += 1000000000L;
        }

	// Print current and elapsed time
        len = snprintf(buf, sizeof(buf),
                       "current time: %lld.%09ld\nelapsed time: %lld.%09ld\n",
                       (long long)current_time.tv_sec, current_time.tv_nsec,
                       (long long)diff.tv_sec, diff.tv_nsec);
    }
    else 
    {
        // Print only the current time on the first read
        len = snprintf(buf, sizeof(buf), "current time: %lld.%09ld\n",
                       (long long)current_time.tv_sec, current_time.tv_nsec);
	first_read = false;
    }

    // Update last_time for the next read
    last_time = current_time;

    return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

// Kernel operations for /proc/timer
static const struct proc_ops timer_fops = {
    .proc_read = timer_read,
};

// Module initialization
static int __init timer_init(void)
{
    // Create /proc/timer entry	
    timer_entry = proc_create(ENTRY_NAME, PERMS, PARENT, &timer_fops);
    if (!timer_entry)
    {
        return -ENOMEM;
    }

    // Initialize time and set first read flag to true
    ktime_get_real_ts64(&last_time);
    first_read = true;
    return 0;
}

// Module removal and cleanup
static void __exit timer_exit(void)
{
    proc_remove(timer_entry);
}

// Register module initialization and removal functions
module_init(timer_init);
module_exit(timer_exit);

