#ifndef __YALNIX_UTILS_H__
#define __YALNIX_UTILS_H__

// utility functions that we add to make our life easier within the yalnix environemtn
static inline unsigned int getKB(unsigned int size) { return size >> 10; }
static inline unsigned int getMB(unsigned int size) { return size >> 20; }
static inline unsigned int getGB(unsigned int size) { return size >> 30; }

#endif