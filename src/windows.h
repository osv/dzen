#ifndef DZEN_WINDOWS_H
#define DZEN_WINDOWS_H

#include "layout.h"

void windows_create(Bool use_ewmh_dock, const ResolvedLayout *layout);
void windows_destroy(void);
void windows_initialize_layout(const ResolvedLayout *layout, Bool horizontal_menu);
void windows_apply_layout(const ResolvedLayout *old_layout, const ResolvedLayout *new_layout, Bool horizontal_menu);
void windows_map_title(void);
void windows_map_slave(void);
void windows_unmap_slave(void);
Bool windows_slave_is_mapped(Bool *mapped);
void windows_set_title_hidden(void);
void windows_raise_all(void);
void windows_lower_all(void);
void windows_resize_expanded_title(int width, int x);
void windows_remember_and_unmap(void);
void windows_restore_mapping(void);
void windows_set_output_available(Bool available);
void windows_update_docking_struts(const ResolvedLayout *layout, const XRectangle *target, const XRectangle *root,
                                   Bool dock_active);
void windows_normal_background_changed(void);
void windows_set_outer_background(Bool visible, unsigned long pixel);

#endif
