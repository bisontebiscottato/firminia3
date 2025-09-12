/*************************************************************
 *                     FIRMINIA 3.5.4                          *
 *  File: qr_image.h                                           *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 *  Description: QR Code image data for BLE configuration     *
 ************************************************************/

#ifndef QR_IMAGE_H
#define QR_IMAGE_H

#include "lvgl.h"

// QR Code image dimensions
#define QR_IMG_WIDTH  100
#define QR_IMG_HEIGHT 100

// QR Code image data (to be populated with actual QR code data)
// This is a placeholder - replace with actual converted QR code data
extern const lv_image_dsc_t qr_code_img;

#endif // QR_IMAGE_H
