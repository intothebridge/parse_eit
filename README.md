# parse_eit
tool for parsing EIT (DVB Event Information Table) files

parse_eit is written in C-language and pretty self contained. 

## Introduction
parse_eit is a really great tool for parsing DVB-S EIT-files. EIT-files are sidecar files that come along with a DVB-S recording (on linux sat receivers).
It seems that this tiny program is "as is" and not further maintained, but there are some problems with this program which need fix.

Reasons for forking @andy1978 's project:
* the _short_event_descriptor_ tag is not enumerated, that makes it impossible to parse the movie-title if there are more than one _short_event_descriptor_
* there is a (serious) design-flaw in Andy's code: every json-block generated has a trailing comma (",") to separate it from the following block. This is fine if everything goes fine. But if there is an parsing-exception (the code looks for that) the program just stops there and exits leaving the so far generated json truncated and incorrect although it may contain useful data. The design flaw is, that the trailing comma *anticipates* that there will be no problem in generating the following block which is unforeseeable. A correct design would first try to generate the correct block and IF successful *precede* it with a comma and add it to the json.
* I did not correct the flow structure in that way but corrected the code in the following way:
*   at the end of program there is ALWAYS a dummy structure being generated so that if generation of a block fails this dummy structure guarantees a correct closing of the json structure
*   furthermore - if there is a parsing error - this dummy structure AND a final curly bracket are being generated at the end of the json.

These modifications dramatically increase the success rate of *parse_eit*

## How to build

Prerequisites:
* some flavour of linux distro
* gcc installed
* build-essentials installed
* sometimes:
*   install libasan
* simply use *make* to generate the executable
* ignore any compiler warnings - Andy's program works great!

## Usage

parse_eit *EIT-File* > out.json

errors go to stderr
output goes to stdout

## Advanced usage

I embedded parse_eit into my shell script "upec.sh" replacing the former bash-based binary processing. So upec now very fast and much better generates hundreds/thousands of NFO-files for DVB-S-recordings found in a directory structure





