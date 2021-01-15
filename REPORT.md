# Project 3 - User Level Thread Part 2  -  + Jacob Porter
###### SID's:
######  DS: 913011681
######  JP: 915371086
#
### High-level Description of Implementation

#### Semaphore Implementation 
The semaphore data structure consists of: 
- Semaphore Value
- Wait Queue 
The semaphore functions are summarized below. Each function is guarded by enter
/exit_critical_section. 

##### sem_create() 
This function allocates the semaphore and initalizes the value to = count. A 
wait queue is created to hold threads waiting for the semaphore. 

##### sem_destroy()
If wait queue is empty this function then destroys the queue and frees up the
semaphore structure. 

##### sem_down() 
This function contains a while loop that checks to see if the semaphore value is
<= 0. If it is then it places the thread on the wait queue and blocks the
thread. Once the thread unblocks, the while check is executed again and if still
less than 0, it goes back into the queue. Once the while loop is exited the
semaphore value is decremented. 

##### sem_up()
This function increments the semaphore value and then checks to see if there are 
any threads in the wait queue. If so, it dequeues the thread and unblocks it. 


##### Semaphore Testing
This implementation passes all the provided test cases. We ran it with extended
parameters (for example, computed primes up to 10,000). 

#### TPS Implementation
##### Data Structures
There is a global data structure which contains a list/queue of all the TPS
blocks. It would have been better to use a hash table to hash thread ID's to TPS
but we didn't have time and wanted to focus on the testing. This means searching
for TPS entries is a linear search.

The two main data structures for each TPS are:
- tps_page contains
    - address of the allocated page
    - reference counter

- tps_block contains 
    - thread ID that owns the TPS
    - Pointer to the tps_page

##### tps_init()
This function allocates the global data structure and created the list/queue for
the TPS's. It also sets up the signal handler for the segmentation faults (TPS
protection faults). 

##### tps_create()
This function checks to see if a TPS already exists and if not, it creates the 
tps_page and tps_block data structure and initalizes them. The page protections
are set to PROT_NONE. Then the tps_block is put into the global TPS list. 

##### tps_destroy()
This function finds the TPS in the global list, checks the reference count and
if it is = 1, it deletes the page and the data strucutres while also removing it 
from the global TPS list. If reference count is > 1, the data page is not
deleted and only the tps_block is deleted. 

##### tps_read()
This function looks up the TPS block address, sets the protection to PROT_READ, 
copies data to the TPS and then resets the function back to PROT_NONE. 

##### tps_write()
This function find the tps_block and if the reference count is > 1, then the
TPS data block is cloned. Once cloned, the data is copied from the original
TPS to the new one. After this, the data to be written is memcpy'd to the tps
data block. The read-write protections of old and new data pages are changed 
only for the clone and write operations. At the end of this the protection is
set to PROT_NONE. 

##### tps_clone() 
This function finds the tps matching the supplied thread ID. A new tps_block is 
created for this thread and is pointed to the existing tps_page. The reference 
counter of tps_page is incremented. The new tps_block is then added to the
global TPS list. 

#### Design Choices
##### Semaphore Choices 
Using a simple data structure and staight forward function design resulted in a 
very solid implementation. 

##### TPS Choices
Using a queue data structure for the global TPS list was done purely for
convenience (the queue ADT was supplied). Given time we would have implemented a
hash table or investigated thread specific memory or keys. All the other data 
structures supported a clean implementation in all the functions. 

### Sources Used for Project
- Utilized the materials and website links provided in the assignment
- Read OSPP for ideas (but the author ReAlLy hates semaphores)
- Utilized some other websites to find examples of mmap(), mprotect(), and the 
signal handlers. 

### Problems we Encountered
- Debugging the semaphore ADT was fairly straightforward
- Debugging the TPS implementation was more difficult
    - Encountered a few bugs in pointer handling 
    - Encountered some bugs in TPS cloning
    - Encountered more bugs in handling TPS protection
        - It was difficult to get all the mprotects() in the precise places that
        they needed to be (restructured code to make this cleaner)

- Tried to create a random TPS tester and it ended up being buggy and horrible
but we are confident on the implementation of everything else. 


