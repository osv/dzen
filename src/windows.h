#ifndef DZEN_WINDOWS_H
#define DZEN_WINDOWS_H

#include "layout.h"

void windows_create(Bool use_ewmh_dock, const ResolvedLayout *layout);
void windows_destroy(void);
void windows_initialize_layout(const ResolvedLayout *layout, Bool horizontal_menu);
void windows_apply_layout(const ResolvedLayout *old_layout, const ResolvedLayout *new_layout, Bool horizontal_menu,
                          Bool title_hidden);

#endif
