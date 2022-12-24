#ifndef __HD44780H__
#define __HD44780H__

#include "controller.h"

struct controller_interface *init_hd47780(struct vfd_dev *dev);

#endif
