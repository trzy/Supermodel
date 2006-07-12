
#ifndef _SCSP_H_
#define _SCSP_H_

extern void scsp_shutdown(void);
extern void scsp_init(UINT8 *scsp_ram, void *sint_hand, void *mint_hand);
extern void scsp_reset(void);

extern void scsp_update(INT32 *bufL, INT32 *bufR, UINT32 len);
extern void scsp_update_timer(UINT32 len);

extern UINT32 scsp_read_8(UINT32 a);
extern UINT32 scsp_read_16(UINT32 a);
extern UINT32 scsp_read_32(UINT32 a);

extern void scsp_write_8(UINT32 a, UINT32 d);
extern void scsp_write_16(UINT32 a, UINT32 d);
extern void scsp_write_32(UINT32 a, UINT32 d);

extern void	scsp_midi_in_send(UINT8 data);
extern void	scsp_midi_out_send(UINT8 data);
extern UINT8 scsp_midi_in_read(void);
extern UINT8 scsp_midi_out_read(void);

extern char * scsp_debug_slist_on(void);
extern char * scsp_debug_ilist(INT s_m, char * out);

#endif /* _SCSP_H_ */

