README for instrument.cc
------

Author: Stanislav Shwartsman (Sourceforge)
------

Modified by: Aneesh Pasricha '16 Amherst College and Scott Kaplan, Professor at Amherst College
-----------

Copyright: 2013
---------

The functions in instrument.cc are called whenever Bochs (version 2.6.2) is run. bx_instr_init_env(void) is called only when Bochs starts, and bx_instr_exit_env(void) is called only when Bochs is closed. void bx_instr_tlb_cntrl(unsigned cpu, unsigned what, bx_phy_address new_cr3) is called whenever a Translation Lookaside Buffer control instruction is executed by Bochs. void bx_instr_lin_access(unsigned cpu, bx_address lin, bx_address phy, unsigned len, unsigned rw) is called whenever Bochs executes an access to memory to perform a read/write operation. Using arguments given to these functions, the functions record virtual address of memory access, cpu timestamp at that instant, whether it was a read or a write operation, virtual address of the least recently used page in an approximate least-recently-used queue (called NRU), and contents of the accessed page and least recently used page.     

Terminal/Console Usage:
---------------------- 
 Usage with Bochs. 
 Configure in the directory which holds all Bochs code: ./configure --enable-instrumentation="instrument/<directory containing modified instrument.cc>"
 Build in the same directory: make install
 Run Bochs: ./bochs 
 The output will appear in listings.txt in that directory.

Where "listings.txt" is a text file containing, in such order, for each memory access:
Read/Write Operation Identifer               (1 byte char)
Virtual Page number of used memory address   (4 byte uint32_t or 8 byte uint64_t for 32- or 64-bit systems)
CPU Timestamp Counter at that memory access  (8 byte long long)
If NRU contains no repitions of this page access, also, in continuing order:
Accessed page image/contents                 (4096 bytes)
If NRU was full, also, in continuing order:
Virtual Page Number of page leaving NRU      (4 byte uint32_t or 8 byte uint64_t for 32- or 64-bit systems) 
Leaving Page Image/Contents                  (4096 bytes)

Explanation of functions:
------------------------
bx_instr_init_env(void) opens listings.txt

bx_instr_exit_env(void) closes listings.txt

void bx_instr_tlb_cntrl(unsigned cpu, unsigned what, bx_phy_address new_cr3): We don't use any of the arguments. We flush out the NRU each time a TLB control occurs in accordance to a TLB flush clearing out the virtual memory.

bx_address queueManager(bx_address lin, bx_address phy_page, bx_address* removed_physical_page): lin is the virtual address of the currently memory access. phy_page is the physical address of the currently accessed page. Makes and Manages the NRU. The NRU is implemented as a clock, with a size of 16 pages. A page enters the NRU whenever it's used and gets marked as 'recently used'. If the page was already present in the NRU it just gets marked as 'recently used'. If the NRU is full, the 'least recently used' page is discarded as the newest page occupies its location. Meanwhile the function returns '0' is there was no addition to the queue, -1 if there was only an addition, and the virtual page number of the removed page if there was a removal. It also modifies the pointer to removed_physical_page to the physical address of the removed page.  

char readWrite(unsigned rw, bx_address casenum): rw is a boolean, 1 means a write operation took place in this access and 0 means read. casenum is the value returned by queueManager. readWrite returns the character to be inserted in the output file as the Read/Write Identifier. The character depends on the casenum (ie, what happened in the queue) so that the ref_trace_reader program knows how many variables to read in (for example, whether it needs to read in the virtual address of the outgoing page).

void writeRecord(unsigned rw1, bx_address lin, long long tsctime, bx_address casenum, BX_CPU_C* ptr, bx_address phy_add, bx_address removed_phy): rw1 is the boolean identifying read/write, tsctime is the cpu timestamp at the instance of memory access, ptr is the pointer to the current CPU, phy_add is the physical page address of the accessed page, removed_phy is removed_physical_page's value. The rest of the arguments are the same as those described above with the same names. It writes in listings.txt (what is described above) using fwrite. It knows how many variables to write using casenum. It obtains the contents of the accessed pages using readPhysicalPage (from BX_MEM(0)), giving the arguments of ptr, phy_add, the size of the contnets and the data file which should hold the contents once obtained. 

void bx_instr_lin_access(unsigned cpu, bx_address lin, bx_address phy, unsigned len, unsigned rw): cpu is the cpu-number of the current CPU. phy is the physical address of memory access. The rest of the arguments are the same as those defined above with the same names. Because Bochs calls this each time a memory access occurs, this calls the queueManager and writeRecord for each memory access resulting in listings.txt containing each of the above described contents for every memory access. It also creates the arguments for these functions. It gets the timestamp and converts the physical memory access address to a physical page number of the access. 

Further information:
-------------------
Comments in the code explain this and much more in great detail. An explanation of the functions already present in instrument.cc which we haven't modified, and extra configuration details, is presented in instrumentation.txt in Bochs sourcecode or online at http://bochs.sourceforge.net/cgi-bin/lxr/source/instrument/instrumentation.txt
 

-----END README----- 
   
