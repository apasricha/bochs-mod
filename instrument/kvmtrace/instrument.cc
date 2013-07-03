/////////////////////////////////////////////////////////////////////////
// $Id: instrument.cc 11312 2012-08-05 13:40:32Z sshwarts $
/////////////////////////////////////////////////////////////////////////
//
//   Copyright (c) 2006-2012 Stanislav Shwartsman
//          Written by Stanislav Shwartsman [sshwarts at sourceforge net]
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

//We added code to track memory accesses, clock time of access, and contents of those accesses' pages.
//bx_address is a long (4-byte) 

#include "bochs.h"
#include "cpu/cpu.h"
#include <iostream>
#include <fstream>
#include "cycle.h"
#include <stdio.h>

#if BX_INSTRUMENTATION

// For a different value of K, add "-D KVMT_K=128", for example, to Makefile.
#if !defined KVMT_K
#define KVMT_K 16
#endif

FILE* myfile;                    //The file we write into
bx_address nru[KVMT_K][3];       //FIFO Queue that holds page numbers of the addresses of memory accesses; Used to make a clock for most recently used pages. Column0 holds virtual page no of memory access, Column1 holds physical page no, Column2 holds boolean for 'just or recently used' 
int nructr = 0;                  //Runs through the Queue as we enter pages into it
int looker = 0;                  //Once the Queue is full, looker runs through to track the page to be taken out to make room for another.

//Open the file to be written in when Bochs starts
void bx_instr_init_env(void) {
  myfile = fopen("listings.txt", "w");
}

//Close the file after writing into it, when Bochs closes
void bx_instr_exit_env(void) {
  fclose(myfile);
}

void bx_instr_initialize(unsigned cpu) {}
void bx_instr_exit(unsigned cpu) {}
void bx_instr_reset(unsigned cpu, unsigned type) {}
void bx_instr_hlt(unsigned cpu) {}
void bx_instr_mwait(unsigned cpu, bx_phy_address addr, unsigned len, Bit32u flags) {}

void bx_instr_debug_promt() {}
void bx_instr_debug_cmd(const char *cmd) {}

void bx_instr_cnear_branch_taken(unsigned cpu, bx_address branch_eip, bx_address new_eip) {}
void bx_instr_cnear_branch_not_taken(unsigned cpu, bx_address branch_eip) {}
void bx_instr_ucnear_branch(unsigned cpu, unsigned what, bx_address branch_eip, bx_address new_eip) {}
void bx_instr_far_branch(unsigned cpu, unsigned what, Bit16u new_cs, bx_address new_eip) {}

void bx_instr_opcode(unsigned cpu, const Bit8u *opcode, unsigned len, bx_bool is32, bx_bool is64) {}

void bx_instr_interrupt(unsigned cpu, unsigned vector) {}
void bx_instr_exception(unsigned cpu, unsigned vector, unsigned error_code) {}
void bx_instr_hwinterrupt(unsigned cpu, unsigned vector, Bit16u cs, bx_address eip) {}

//Flush the queue out each time a TLB control occurs
void bx_instr_tlb_cntrl(unsigned cpu, unsigned what, bx_phy_address new_cr3) {
  for(int i = 0; i < KVMT_K; i++) {
    for(int j = 0; j < 3; j++) {
      nru[i][j] = 0;
    }
  }
}

void bx_instr_clflush(unsigned cpu, bx_address laddr, bx_phy_address paddr) {}
void bx_instr_cache_cntrl(unsigned cpu, unsigned what) {}
void bx_instr_prefetch_hint(unsigned cpu, unsigned what, unsigned seg, bx_address offset) {}

void bx_instr_before_execution(unsigned cpu, bxInstruction_c *i) {}
void bx_instr_after_execution(unsigned cpu, bxInstruction_c *i) {}
void bx_instr_repeat_iteration(unsigned cpu, bxInstruction_c *i) {}

void bx_instr_inp(Bit16u addr, unsigned len) {}
void bx_instr_inp2(Bit16u addr, unsigned len, unsigned val) {}
void bx_instr_outp(Bit16u addr, unsigned len, unsigned val) {}

//Makes a Clock, and manages the presence of pages in the Queue. Handles entry and removal (if needed) of virtual and physical page numbers of each memory access. Column2 (ie, the third column) holds a boolean (disguised as an int, 1 for true and 0 for false) which signifies 'recently used'. It is made true each time its page is referred to. Once the queue is full and a page is used and needs to be added to the queue, we look for the least recently used page to remove and make room for the new page. As we look for a place to add, each page we look through has its boolean falsified (if it was true) because now it is less recently used than it once was. If the examination chances upon an already falsified boolean, the removal and refilling occurs there. Examination restarts from that spot when a new page is used and needs to be added to the queue.  
bx_address queueManager(bx_address lin, bx_address phy_page, bx_address* removed_physical_page) {
  bx_address pagenum = lin >> 12;                      //page number of virtual memory address
  bool isThere = false;                                //Checks whether accessed page is already present in the queue
  for(int i = 0; i < nructr; i++) {
    if(nru[i][0] == pagenum) {
      isThere = true;
      nru[i][2] = 1;                                  //If it is, make Column2 of the clock true, for 'just used'
      return (bx_address)(0);                         //And return it as case no. 0 (neither addition nor removal)
    }
  }
  if(!isThere) {                                     //If the page is not already present
    if(nructr < KVMT_K) {                            //If there is room in the queue 
      nru[nructr][0] = pagenum;
      nru[nructr][1] = phy_page;                     //Enter the virtual and physical page no into it
      nru[nructr][2] = 1;                            //Make 'recently used' true
      nructr++;                                      //Increment to next queue position
      return (bx_address)(-1);                       //Return 'only addition'
    } 
    else {                                           //Queue full
      while(nru[looker][2] == 1) {                   //Searching for least recently used page to remove
	nru[looker][2] = 0;                          //As we look through, each page examined is then 'less recently used', boolean falsified
	if(looker == KVMT_K-1) looker = 0;           //If we reach end of queue, wrap-around
	else looker++;                               //If not, increment
      }                                              //Now looker stops at a page where boolean is already 'false' that is, 'least recently' used
      bx_address oldnum = nru[looker][1];            //record phy add of page to be removed
      bx_address outvirtualnum = nru[looker][0];     //record lin add of page to be removed
      nru[looker][0] = pagenum;                      //put new page there
      nru[looker][1] = phy_page;
      nru[looker][2] = 1;                            //now that page is 'recently used'
      *removed_physical_page = oldnum;
      return outvirtualnum;                          //return the lin page no of removed page
    }
  }
}

//Returns a read/write access identifier, little letter for no addition/removal, else capital
char readWrite(unsigned rw, bx_address casenum) {
  if(casenum == 0) {
    if(rw & 1) return 'w';
    else return 'r';
  }
  if(casenum == -1) {
    if(rw & 1) return 'W';
    else return 'R';
  }
  if(rw & 1) return 'Q';
  else return 'T';
}

//writes the access address, read/write identifer, clock time of access and page contents of added (if any) or removed (if any) pages
void writeRecord(unsigned rw1, bx_address lin, long long tsctime, bx_address casenum, BX_CPU_C* ptr, bx_address phy_add, bx_address removed_phy) {
  static void* page_contents = malloc(4096);                                 //pointer to data holder for page contents
  char rw = readWrite(rw1, casenum);                                         //gets read/write identifier
  fwrite(&rw, sizeof(rw), 1, myfile);                                        //write read write identifer, linear access address, and clock time for each case
  fwrite(&lin, sizeof(lin), 1, myfile);
  fwrite(&tsctime, sizeof(tsctime), 1, myfile);
  //std::cout << "size of : " << sizeof(rw) << " " << sizeof(lin) << " " << sizeof(tsctime) << std::endl;
  if(casenum == 0) return;                                                  //if neither addition nor removal, exit method
  BX_MEM(0)->readPhysicalPage(ptr, phy_add, 4096, page_contents);           //if not, look at contents of added page and store in page_contents
  fwrite(page_contents, 4096, 1, myfile);                                   //and write that to file
  if(casenum == -1) return;                                                 //if no removal, exit
  fwrite(&casenum, sizeof(casenum), 1, myfile);
  BX_MEM(0)->readPhysicalPage(ptr, removed_phy, 4096, page_contents);       //if removal, store content of removed page
  fwrite(page_contents, 4096, 1, myfile);                                   //and write to file
}

//Calls the rest of the methods which handle recording to file
void bx_instr_lin_access(unsigned cpu, bx_address lin, bx_address phy, unsigned len, unsigned rw) {
  long long timer = (long)(bx_pc_system.time_ticks());                            //records clock time at access
  bx_address page_add_mask = -1;                                             //Process of conversion of physical memory access location to access page number
  page_add_mask = page_add_mask << 12;
  bx_address phy_page_add = phy & page_add_mask;
  BX_CPU_C* cpu_ptr = BX_CPU(cpu);                                            //Pointer to current CPU used
  bx_address removed_physical_page;
  bx_address casenum = queueManager(lin, phy_page_add, &removed_physical_page); //gets case (of removal or addition of page to queue)
  writeRecord(rw, lin, timer, casenum, cpu_ptr, phy_page_add, removed_physical_page);                //and writes to file
}

void bx_instr_phy_access(unsigned cpu, bx_address phy, unsigned len, unsigned rw) {}

void bx_instr_wrmsr(unsigned cpu, unsigned addr, Bit64u value) {}

#endif
