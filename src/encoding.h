#ifndef SCREEN_ENCODING_H
#define SCREEN_ENCODING_H

void  InitBuiltinTabs (void);
struct mchar *recode_mchar (struct mchar *, int, int);
struct mline *recode_mline (struct mline *, int, int, int);
int   FromUtf8 (int, int *);
void  AddUtf8 (int);
int   ToUtf8 (char *, int);
int   ToUtf8_comb (char *, int);
int   utf8_isdouble (int);
int   utf8_iscomb (int);
void  utf8_handle_comb (int, struct mchar *);
int   ContainsSpecialDeffont (struct mline *, int, int, int);
int   LoadFontTranslation (int, char *);
void  LoadFontTranslationsForEncoding (int);
void  WinSwitchEncoding (struct win *, int);
int   FindEncoding (char *);
char *EncodingName (int);
int   EncodingDefFont (int);
void  ResetEncoding (struct win *);
int   CanEncodeFont (int, int);
int   DecodeChar (int, int, int *);
int   RecodeBuf (unsigned char *, int, int, int, unsigned char *);
int   PrepareEncodedChar (int);
int   EncodeChar (char *, int, int, int *);

#endif /* SCREEN_ENCODING_H */
