#ifndef __PCD8544H__
#define __PCD8544H__

#include "controller.h"

struct controller_interface *init_pcd8544(struct vfd_dev *_dev);

#endif
