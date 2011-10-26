/*
 * Module:      mccp.h
 * Owner:       Satori
 * Checked-in:  04 Oct, 2000
 * Version:     0.0.0a
 *
 * Mud Client Compression Protocol support header file.  We define
 * the elusive TELOPT_COMPRESS here, since it's a non-standard TELNET
 * control sequence.
 */

#ifndef GZ2_MCCP_H_
#define GZ2_MCCP_H_

#define MAX_COMPRESS_LEN    16384

bool begin_compression(struct descriptor_data *d);
bool end_compression(struct descriptor_data *d);
bool process_compressed_output(struct descriptor_data *d);
bool write_compressed(struct descriptor_data *d, const char *txt, int len);

DECLARE_DO_FUN(do_compress);

#endif                          /* GZ2_MCCP_H_ */
