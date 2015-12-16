#ifndef __MACH_LS1021A_H
#define __MACH_LS1021A_H

#ifdef CONFIG_SOC_LS1021A
int is_ls1021a_rev1(void);
#else
inline int is_ls1021a_rev1(void) { return 0; }
#endif

#endif
