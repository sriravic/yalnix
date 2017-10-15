/**
File: pagetable.h
*****************
Team Zoidberg
*/

#ifndef __PAGETABLE_H__
#define __PAGETABLE_H__

#include <hardware.h>

// The page table is a linked list for keeping track of pages a particular process has.
// The page table is on a per process basis
struct pagetable
{
	struct pte m_entry;             // the pte entry 
    struct pagetable* m_next;       // pointer to the next entry in the page tables
};

typedef struct pagetable PageTable;

// global datastructure
// store all the pagetables in this linked list
PageTable* gPageTables;

#endif
