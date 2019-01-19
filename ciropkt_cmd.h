/**
 * File: ciropkt_cmd.h
 * 
 * Description: Ciropkt commands and packets.
 * 
 * Author: pepemanboy
 */

//// INCLUDES

#include <stdint.h>

#ifndef CIROPKT_CMD_H
#define CIROPKT_CMD_H

#ifdef __cplusplus
extern "C" {
#endif

/** Commands */
typedef enum
{
  cmd_CPGInfo = 0,
  cmd_MCGControl,
}cmds_e;

/** CPG info structure */
typedef struct CPGInfo CPGInfo;
struct CPGInfo
{
  uint8_t cpg_id;
};

/** Montacargas structure */
typedef enum
{
  mcg_On = 0,
  mcg_Off,
}mcg_e;
typedef struct MCGControl MCGControl;
struct MCGControl
{
  uint8_t cmd;
};

#ifdef __cplusplus
}
#endif

#endif // CIROPKT_CMD_H


