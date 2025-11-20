/*!
  \file eit_parse.c
  \author Andreas Weber

  tool for parsing EIT (DVB Event Information Table) files
  Copyright (C) 2016..2023 Andreas Weber

  Gibt die Informationen ine einer DreamBox 7025+ (vielleicht auch andere)
  .eit Datei als Text aus.

  Probleme:
    - den Text muss man escapen, da auch " darin vorkommt, siehe Schneewelt1.eit
  
  20251120 Modifications by geargineer - see https://github.com/intothebridge/parse_eit/blob/master/README.md

*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <iconv.h>
#include <inttypes.h>
#include <assert.h>

//#define DEBUG

struct s_duration
{
  int hour;
  int minute;
  int second;
};

/*
  5.2.4 Event Information Table (EIT) : Seite 35

  duration: A 24-bit field containing the duration of the event in hours, minutes, seconds. format: 6 digits,
  4-bit BCD = 24 bit.

  EXAMPLE 3:
    01:45:30 is coded as "0x014530".
*/

uint8_t parse_duration (const uint8_t *p, size_t len, struct s_duration *s)
{
  if (len < 3)
    return 0;

  s->hour   = (p[0] >> 4) * 10 + (p[0] & 0x0F);
  s->minute = (p[1] >> 4) * 10 + (p[1] & 0x0F);
  s->second = (p[2] >> 4) * 10 + (p[2] & 0x0F);
  return 3;
}

struct s_start_time
{
  int Y;
  int D;
  int M;

  struct s_duration t;   // ist eigentlich die Startzeit, hat aber gleiches Format wie duration
};

/*
  5.2.4 Event Information Table (EIT) : Seite 35

  start_time: This 40-bit field contains the start time of the event in Universal Time, Co-ordinated (UTC) and Modified
    Julian Date (MJD) (see annex C). This field is coded as 16 bits giving the 16 LSBs of MJD followed by 24 bits coded as
    6 digits in 4-bit Binary Coded Decimal (BCD). If the start time is undefined (e.g. for an event in a NVOD reference
    service) all bits of the field are set to "1".

  EXAMPLE 1:
    93/10/13 12:45:00 is coded as "0xC079124500".
*/

uint8_t parse_start_time (const uint8_t *p, size_t len, struct s_start_time *s)
{
  if (len < 5)
    return 0;

  // EXAMPLE 1 from above
  //p[0] = 0xC0;
  //p[1] = 0x79;

  // Seite 145: Annex C
  int MJD = p[0] << 8 | p[1];

  int Ys = (MJD - 15078.2) / 365.25;
  int tmp = (Ys * 365.25);
  int Ms = (MJD - 14956.1 - tmp) / 30.6001;
  int tmp1 = (Ys * 365.25);
  int tmp2 = (Ms * 30.6001);
  s->D = MJD - 14956 - tmp1 - tmp2;
  int K = (Ms == 14 || Ms == 15)? 1 : 0;
  s->Y = Ys + K;
  s->M = Ms - 1 - K * 12;

  //printf ("MJD = %i\n");
  //printf ("Y/M/D = %i/%i/%i\n", s->Y, s->M, s->D);

  parse_duration (p + 2, len - 2, &s->t);
  //printf ("%02i:%02i:%02i\n", s->t.hour, s->t.minute, s->t.second);

  return 5;
}

/*
void dump (const uint8_t *p, size_t len)
{
  for (unsigned int k = 0; k < len; ++k)
    printf ("%3u : 0x%02x  %3i '%c'\n", k, p[k], p[k], p[k]);
}
*/

// gibt die code_table für iconv zurück, aktualisiert p und len
size_t get_code_table (char *p, size_t len, char **code_table)
{
  size_t ret = 0;
  //fprintf (stderr, "DEBUG crop_code_table: len = '%i'\n", *len);

  // Annex A, Seite 130
  // A.2 If the first byte of the text field has a value in the range "0x20" to "0xFF"
  // then this and all subsequent bytes in the text item are coded using
  // the default character coding table (table 00 - Latin alphabet)
  *code_table = "ISO-8859-1";

  if (len >= 1)
    {
      uint8_t first_byte_value = p[0];
      //printf ("first_byte_value = 0x%02x\n", first_byte_value);
      if (first_byte_value < 0x20)
        {
          ret++;
          switch (first_byte_value)
            {
            case 0x01:
              *code_table = "ISO-8859-5";
              break;
            case 0x02:
              *code_table = "ISO-8859-6";
              break;
            case 0x03:
              *code_table = "ISO-8859-7";
              break;
            case 0x04:
              *code_table = "ISO-8859-8";
              break;
            case 0x05:
              *code_table = "ISO-8859-9";
              break;
            case 0x06:
              *code_table = "ISO-8859-10";
              break;
            case 0x07:
              *code_table = "ISO-8859-11";
              break;
            case 0x09:
              *code_table = "ISO-8859-13";
              break;
            case 0x0A:
              *code_table = "ISO-8859-14";
              break;
            case 0x0B:
              *code_table = "ISO-8859-15";
              break;
            case 0x11:
              *code_table = "ISO-10646";
              break;
            case 0x13:
              *code_table = "GB2312";
              break;
            case 0x15:
              *code_table = "ISO-10646/UTF8";
              break;
            default:
              break;
            }

          if (first_byte_value == 0x10) // dynamically selected part of ISO/IEC 8859
            {
              if (len >= 3)
                {
                  uint8_t second_byte_value = p[1];
                  assert (second_byte_value == 0x00);  // Table A.4

                  uint8_t third_byte_value = p[2];
                  ret += 2;

                  switch (third_byte_value)
                    {
                    case 0x01:
                      *code_table = "ISO-8859-1";
                      break;
                    case 0x02:
                      *code_table = "ISO-8859-2";
                      break;
                    case 0x03:
                      *code_table = "ISO-8859-3";
                      break;
                    case 0x04:
                      *code_table = "ISO-8859-4";
                      break;
                    case 0x05:
                      *code_table = "ISO-8859-5";
                      break;
                    case 0x06:
                      *code_table = "ISO-8859-6";
                      break;
                    case 0x07:
                      *code_table = "ISO-8859-7";
                      break;
                    case 0x08:
                      *code_table = "ISO-8859-8";
                      break;
                    case 0x09:
                      *code_table = "ISO-8859-9";
                      break;
                    case 0x0A:
                      *code_table = "ISO-8859-10";
                      break;
                    case 0x0B:
                      *code_table = "ISO-8859-11";
                      break;
                    case 0x0D:
                      *code_table = "ISO-8859-13";
                      break;
                    case 0x0E:
                      *code_table = "ISO-8859-14";
                      break;
                    case 0x0F:
                      *code_table = "ISO-8859-15";
                      break;
                    default:
                      break;
                    }
                }
              else
                {
                  fprintf (stderr, "ERROR: dynamically selected part of ISO/IEC 8859 but len = %zu (<3)\n", len);
                  exit (-1);
                }
            }
        }
    }
  //fprintf (stderr, "DEBUG code_table = '%s'\n", *code_table);

  return ret;
}

void print_JSON_escaped (const char *p)
{
  while (*p)
    {
      if (*p == '"' || *p == '\\' || ('\x00' <= *p && *p <= '\x1f'))
        printf ("\\u%04x", (int)*p);
      else
        putchar (*p);
      p++;
    }
}

void dump_text (uint8_t *p, size_t len, char append)
{
  size_t outbytesleft = 2048;
  char *outbuf = (char *) malloc (outbytesleft);

  char *code_table;
  size_t inc = get_code_table (p, len, &code_table);

  // get_code_table gibt die Anzahl Zeichen zurück, die für die code Tabelle verwendet wurden (zwischen 0 und 3 Byte)
  //printf ("DEBUG: inc = %zi, code_table = '%s'\n", inc, code_table);
  p += inc;
  len -= inc;

  iconv_t cd = iconv_open ("UTF−8", code_table);
  if (cd == (iconv_t) -1)
    {
      fprintf (stderr, "iconv_open failed: %i = '%s'\n", errno, strerror (errno));
      exit (-1);
    }

  static char *bytes_left = 0;

  if (append && bytes_left)
    {
      size_t num_bytes_left = strlen (bytes_left);
      p -= num_bytes_left;
      strncpy (p, bytes_left, num_bytes_left);
    }

  if (bytes_left)
    free (bytes_left);
  bytes_left = 0;

  char *pout = outbuf;
  char *pin = p;

  // copy machen und die letzten Zeichen anzeigen
  //~ uint8_t temp[len + 1];
  //~ strncpy (temp, p, len);
  //~ temp[len] = 0;
  //~ fflush (stdout);
  //~ fprintf (stderr, "temp = '%s'\n", temp);
  //~ fprintf (stderr, "%#x\n", temp[len - 3]);
  //~ fprintf (stderr, "%#x\n", temp[len - 2]);
  //~ fprintf (stderr, "%#x\n", temp[len - 1]);
  //~ fprintf (stderr, "#x\n", temp[len]);

  iconv (cd, NULL, NULL, &pout, &outbytesleft);
  size_t nconv = iconv (cd, &pin, &len, &pout, &outbytesleft);

  if (nconv == (size_t) -1)
    {
      if (errno == EINVAL)
        {
          // das kann vorkommen, wenn im "Sonderzeichen" auf extended_event_descriptor gesplittet wurde
          // z.B. ./samples/20190218_2139__ProSieben__The_Big_Bang_Theory.eit
          bytes_left = strndup (pin, len);
        }
      else
        {
          fprintf (stderr, "ERRROR: iconv failed with code %i: '%s'", errno, strerror (errno));

          if (errno == EILSEQ)
            {
              fprintf (stderr, ": invalid multibyte sequence '%s' at index %i\n", pin, (uint8_t*) pin - p);
              exit (-1);
            }
          else if (errno == E2BIG)
            {
              fprintf (stderr, ": output buffer too small\n");
              exit (-1);
            }
          else
            fprintf (stderr, "\n");
        }
    }

  *pout = 0;

  //printf ("outbuf = '%s'\n", outbuf);
  //assert (nconv == 0);
  //printf ("%s", outbuf);

  // FIXME: Die CR/LF Ersetzung sollte man besser vorher machen
  // andererseits steht da, das wäre die utf-8 sequence...

  // wtf? 0xC28A ist wohl CR/LF, Seite 130 Annex A

  //~ for (unsigned int k = 0; k < (len - (code_table > 0)); ++k)
  //~ {
  //~ // FIMXE: check for buffer end
  //~ if (   p[k] == 0xC2
  //~ && k +1 < (len - (code_table > 0))
  //~ && p[k+1] == 0x8A)
  //~ k++; //ignore CR/LF
  //~ else
  //~ printf ("%c", p[k]);
  //~ }


  if (iconv_close (cd) != 0)
    perror ("iconv_close");

  print_JSON_escaped (outbuf);

  free (outbuf);

}

int main (int argc, char *argv[])
{
  // geargineer: counter fuer die short events eingefuehrt, um diese im json unterscheiden zu koennen
  int shortevent_count = 0;
  if (argc < 2)
    {
      fprintf (stderr, "ERROR: No input file...\n\nUSAGE: %s EIT\n", argv[0]);
      exit (-1);
    }

  int num_files = argc - 1;

  if (num_files > 1)
    printf ("[\n");

  FILE *fp;
  for (int k = 1; k < argc; ++k)
    {
      const char *fn = argv[k];
      // print opening bracket
      printf (" {\n");

      printf ("  \"filename\": \"%s\",\n", fn);

      fp = fopen (fn, "rb");
      if (!fp)
        {
          fprintf (stderr, "error opening file %s\n", fn);
          exit(-1);
        }

      // Die EITs die bei mir so rumliegen, haben max 1100 byte
#define BUF_SIZE 2000
      uint8_t buf[BUF_SIZE];
      size_t num = fread (buf, 1, BUF_SIZE, fp);
      //printf ("  \"num_bytes\": %i,\n", num);

      if (num == BUF_SIZE)
        {
          fprintf (stderr, "ERROR: Buffer zu klein. Möglicherweise ist das gar kein EIT...\n");
          exit (-1);
        }

      fclose(fp);

      uint8_t *p = buf;

      // 5.2.4 Event Information Table (EIT), Seite 35:
      int event_id = p[0] << 8 | p[1];
      p += 2;

      struct s_start_time st;
      uint8_t r = parse_start_time (p, num - (p - buf), &st);
      p += r;

      struct s_duration dur;
      r = parse_duration (p, num - (p - buf), &dur);
      p += r;

      // running_status
      // undefined = 0, not_running, starts_in_a_few_seconds, pausing, running, serive_off_air, reserved1, reserved2

      uint8_t running_status = p[0] & 0x03;
      uint8_t free_CA_mode   = (p[0] >> 3) & 0x01;
      uint16_t descriptors_loop_length = (p[0] & 0xF0) << 8 | p[1];

      printf ("  \"event_id\": %i,\n", event_id);
      printf ("  \"start_time\": \"%i/%i/%i %02i:%02i:%02i\",\n", st.Y, st.M, st.D, st.t.hour, st.t.minute, st.t.second);
      printf ("  \"duration\": \"%02i:%02i:%02i\",\n", dur.hour, dur.minute, dur.second);
      printf ("  \"running_status\": %i,\n", running_status);
      printf ("  \"free_CA_mode\": %i,\n", free_CA_mode);

      //printf ("descriptors_loop_length = %i\n", descriptors_loop_length);

      p += 2;

      // Seite 39, Tabelle 12
#define SHORT_EVENT_DESCRIPTOR 0x4d
#define EXTENDED_EVENT_DESCRIPTOR 0x4e
#define COMPONENT_DESCRIPTOR 0x50

      uint8_t last_descriptor_tag = 0;
      char first_descriptor = 1;
      while (p < (buf + num))
        {
          uint8_t descriptor_tag = p[0];
          uint8_t descriptor_length = p[1];

          //fprintf (stderr, "Bytes left: %li\n", buf + num - p);
          //fprintf (stderr, "descriptor_tag = %#x\n", descriptor_tag);
          //fprintf (stderr, "descriptor_length = %i\n", descriptor_length);  // Länge der folgenden Daten in Bytes

          p += 2;

          // Seite 87, Kapitel 6.2.37 : Short event descriptor
          if (descriptor_tag == SHORT_EVENT_DESCRIPTOR)
            {
              shortevent_count += 1;
              printf ("  \"short_event_descriptor_%i\":\n  {\n", shortevent_count);
              printf ("    \"iso_639_2_language_code\": \"%c%c%c\",\n", p[0], p[1], p[2]);

              p += 3;

              uint8_t event_name_length = p[0];
              //printf ("    \"event_name_length\": %i,\n", event_name_length);
              p += 1;

              // printf ("    \"event_name_%i\": \"", shortevent_count);
              printf ("    \"event_name\": \"");
              dump_text (p, event_name_length, 0);
              printf ("\",\n");

              p += event_name_length;

              uint8_t text_length = p[0];
              //printf ("    \"text_length\": %i,\n", text_length);
              p += 1;

              printf ("    \"text\": \"");
              dump_text (p, text_length, 0);
              // Change! 20251120 - if program exits AFTER this descriptor, the comma in printf makes the json invalid --> removed
//              printf ("\"\n  },\n");
              printf ("\"\n  },\n");
              p += text_length;
            }
          // Seite 64, Kapitel 6.2.15 : Extended event descriptor
          else if (descriptor_tag == EXTENDED_EVENT_DESCRIPTOR)
            {
              //printf ("EXTENDED_EVENT_DESCRIPTOR\n");

              uint8_t descriptor_number = p[0] >> 4;
              uint8_t last_descriptor_number = p[0] & 0x0F;
              p += 1;

#ifdef DEBUG
              printf ("descriptor_number = %i\n", descriptor_number);
              printf ("last_descriptor_number = %i\n", last_descriptor_number);
#endif

              if (descriptor_number == 0)
                {
                  printf ("  \"extended_event_descriptor\":\n  {\n");
                  printf ("    \"iso_639_2_language_code\": \"%c%c%c\",\n", p[0], p[1], p[2]);
                  // IWi 20251107: um den text im extended descriptor zu identifizieren den key von 'text' auf 'text_extended' gesetzt
                  // printf ("    \"text_extended\": \"");
                  printf ("    \"text\": \"");
                }

              p += 3;

              // Tabelle 53, Seite 64
              uint8_t length_of_items = *(p++);   // kann auch 0 sein
              // printf ("length_of_items = %i\n", length_of_items);

              if (length_of_items > 0)
                {
                  fprintf (stderr, "Noch nicht implementiert...\n");
                  exit (-1);
                }

              uint8_t text_length = *(p++);
              //printf ("text_length = %i\n", text_length);

              dump_text (p, text_length, descriptor_number > 0);

              // Sind wir am Ende?
              if (descriptor_number == last_descriptor_number)
                // geargineer 20251120 added comma to terminate json-structure before next structure
                printf ("\"\n  },\n");

              p += text_length;
            }
          // Seite 46, Kapitel 6.2.8
          else if (descriptor_tag == COMPONENT_DESCRIPTOR)
            {
              uint8_t stream_content_ext = p[0] >> 4;
              uint8_t stream_content = p[0] & 0x0F;
              uint8_t component_type = p[1];
              uint8_t component_tag = p[2];

#ifdef DEBUG
              printf ("COMPONENT_DESCRIPTOR\n");
              printf ("stream_content_ext = %i\n", stream_content_ext);
              printf ("stream_content = %i\n", stream_content);
              printf ("component_type = %i\n", component_type);
              printf ("component_tag = %i\n", component_tag);
              printf ("iso_639_2_language_code = \"%c%c%c\"\n", p[3], p[4], p[5]);
#endif
              p += 6;

              //if (last_descriptor_tag != COMPONENT_DESCRIPTOR)
              //  printf ("  \"component_descriptor\":\n  {\n");

              //printf ("    \"text\": \"");
              // hier keine Länge, muss man sich wohl aus descriptor_length berechnen
              size_t len = descriptor_length - 6;
              //dump_text (p, len, 0);

              //printf ("\"\n  }\n");
              p += len;

            }
          else
            {
              int bytes_left = buf + num - p;
              if (bytes_left > 0)
                {
                  fprintf (stderr, "Unbekannter descriptor_tag %#x, descriptor_length=%i, bytes left = %i\n", descriptor_tag, descriptor_length, bytes_left);
                  //print emtpy structure to get valid json befor exiting
                  printf ("  \"empty_structure\":\n");
                  printf ("  {\n");
                  printf ("    \"dummy\": \"nix\" \n");
                  printf ("  }\n");

                  // print closing bracket for valid json
                  printf (" }\n");
                  exit (-1);
                }
            }

          last_descriptor_tag = descriptor_tag;
          first_descriptor = 0;
        }

#ifdef DEBUG
      printf ("End: Bytes left: %li\n", buf + num - p);
#endif
      // regular termination of program:
      printf ("  \"empty_structure\":\n");
      printf ("  {\n");
      printf ("    \"dummy\": \"nix\" \n");
      printf ("  }\n");

      printf (" }%s\n", (k < argc - 1)? "," : "");
    }
  if (num_files > 1)
    printf ("]\n");
  return 0;
}


