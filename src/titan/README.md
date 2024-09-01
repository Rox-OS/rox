# Titan
Titan is an experimental kernel written in Biron used for the Rox operating system.

## The Idea
The idea behind Titan is to recognize that modern CPUs contain more than one physical core and that the kernel itself can run unconstrained on one of those cores without ever being interrupted by userspace.

The main benefits of this design are
  * No context switching - Since the kernel is always running there is no need to interleave userspace applications with kernel space. This means you do not need to backup and restore state or transition between ring levels. There is no system calls.
  * Security - Since the kernel is pegged to a single CPU core which never runs userspace code and never transitions into userspace, there is a clear separation between userspace and kernel space. There is no need for security mitigation techniques like kernel page table isolation, kernel address space layout randomization, or using process context ids to optimizes TLB flushes. Titan does not use hyper-threading for similar reasons and instead only exposes physical cores.
  * Safety - Traditional kernel design requires code be written in both a reentrant-safe and thread-safe way since multiple processors can transition into kernel code in parallel. Titan dedicates a single physical core to the kernel and runs it in a mostly single-threaded fashion. This greatly simplifies the way the kernel code is written and leads to less bugs.
  - Floating point - Kernels in the past have avoided supporting floating point operations due to the amount of architectual state which would need to be backed up and restored on context switch. Since Titan has no context switch, this makes it possible to enable and use floating point operations in the kernel.

### How are system calls made

When userspace needs to communicate with Titan it does so through two 4 MiB ring buffers mapped in the userspace process backed by two 4 MiB pages. One is called the submission queue and the other is called the completion queue. Userspace is responsible for writing requests onto the submission queue for the kernel to observe. The kernel will pickup submissions when ever it sees fit and will process them when ever it sees fit. When it's finished with a submission it will put the result onto the completion queue to let userspace know. It will also signal a condition variable held in the submission queue for that request so that userspace can know.

### How does scheduling work

The kernel core itself has no scheduler since the kernel always runs. However, each additional physical core runs the scheduler (as opposed to the full kernel). The scheduler runs in userspace as a privlidged process. You may think of the scheduler as `init` except each physical core (apart from the kernel core) executes it. The scheduler also communicates with the kernel through the same ring buffer mechanism to be notified of new processes to schedule and when to migrate processes to a different scheduler core.