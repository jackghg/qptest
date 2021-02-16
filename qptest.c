// qptest 0.5
// g++ qptest.c -o qptest `pkg-config --cflags gtk+-2.0` `pkg-config --libs gtk+-2.0` -lasound
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <algorithm>
#include <sys/signal.h>
#include <sys/types.h>
#include <alsa/asoundlib.h>

using namespace std;
// --- VARIABLES TO CUSTOMIZE ---
bool da=1; // audio, 0 if the modem module has it's own audio chip
bool uac=1; // UAC mode:uses a usb audio devices. If 0 a stream will be used.
const char *pdev = "plughw:0,0"; // playback device
const char *rdev = "plughw:0,0";// record device
const char *updev = "plughw:1,0"; //  uac playback device
const char *urdev = "plughw:1,0"; // uac record device
const char *adev = "/dev/ttyUSB1"; // audio serial port
const char *cdev = "/dev/ttyUSB2"; // main serial port (AT commands)
char np = 'i'; // 'I' to call with private number
// ---
int fd = -1;
char nums[] = "0123456789";
char numc[] = "0123456789*#+ ";
// UTF8 / GSM charset conversion
char UGc[51][4] = {{0,0,64,0},{1,0,194,163},{2,0,36,0},{3,0,194,165},{4,0,195,168},{5,0,195,169},{6,0,195,185},{7,0,195,172},{8,0,195,178},{9,0,195,135},{11,0,195,152},{12,0,195,184},{14,0,195,133},{15,0,195,165},{16,0,206,148},{17,0,95,0},{18,0,206,166},{19,0,206,147},{20,0,206,155},{21,0,206,169},{22,0,206,160},{23,0,206,168},{24,0,206,163},{25,0,206,152},{26,0,206,158},{27,20,94,0},{27,40,123,0},{27,41,125,0},{27,47,92,0},{27,60,91,0},{27,61,126,0},{27,62,93,0},{27,64,124,0},{27,101,226,0},{28,0,195,134},{29,0,195,166},{30,0,195,159},{31,0,195,137},{36,0,194,164},{64,0,194,161},{91,0,195,132},{92,0,195,150},{93,0,195,145},{94,0,195,156},{95,0,194,167},{96,0,194,191},{123,0,195,164},{124,0,195,182},{125,0,195,177},{126,0,195,188},{127,0,195,160}};
int cs=51;
char * t;
char * fi;
bool ubsy=0; // busy status because of a incoming message
bool cbsy=0; // busy status because of a command execution
bool pst=0;
bool rst=0;
bool su=0;
int u=0;
char cst=0; // Call status
// 0 = closed
// 1 = outgoing call ( calling )
// 2 = incoming call ( ringing )
// 3 = outgoing call open
// 4 = incoming call open
GtkWidget *window;
GtkWidget *ennum;
GtkWidget *enpi;
GtkWidget *tvtxt;
GtkWidget *stat;
GtkTextBuffer *txtbuf;
//GThread *pth, *rth;
pid_t pidp=-2;
pid_t pidr=-2;
gint sc;
string ext="x";
// Status bar updater
static void sstat( string sm ) {
const char *cs = sm.c_str();
gtk_statusbar_push(GTK_STATUSBAR(stat), GPOINTER_TO_INT(sc), cs); }
// Audio stream in
static gpointer audioi(gpointer p) {
usleep(700000);
// termios setup
int fa, ll;
struct termios aty;
unsigned char ibuf[1301];
fa = open(adev, O_RDONLY | O_NOCTTY); 
if (fa < 0) cout << endl << "Playback Audio Serial Error!\n" << endl; else cout << "Playback Audio Serial OK!\n" << endl;
if (tcgetattr(fa, &aty) < 0) cout << "playback serial tcgetattr error\n" << endl;
aty.c_cflag = CS8 | CLOCAL | CREAD;
aty.c_cflag &= ~(CSTOPB | CRTSCTS | PARENB | HUPCL);
aty.c_iflag = IGNPAR | IGNBRK;
aty.c_iflag &= ~(BRKINT | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY | INPCK);
aty.c_oflag = 0;
aty.c_lflag = 0;
cfsetispeed(&aty, B115200);
aty.c_cc[VTIME] = 0;
aty.c_cc[VMIN] = 0;
if (tcsetattr(fa, TCSANOW, &aty) < 0) cout << "Error from play audio tcsetattr\n" << endl;
// alsa setup
snd_pcm_t *han;
snd_pcm_sframes_t fr;
if (snd_pcm_open(&han, pdev, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0) cout << "Audio Device Open Error!" << endl; else cout << "Playback device Ok!\n" << endl;
if (snd_pcm_set_params(han, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 1, 8000, 1, 200000) < 0) cout << "Audio Params Error!" << endl;
snd_pcm_prepare(han);
tcflush(fa, TCIFLUSH);
// loop read from modem, writes to alsa
while (pst) {
ll = read(fa, ibuf, 1300);
fr = snd_pcm_writei(han, ibuf, ll);
if (fr < 0) { fr = snd_pcm_recover(han, fr, 0); }
if (fr < 0) { printf("snd_pcm_writei failed: %s\n", snd_strerror(fr)); }
usleep(79000);
}
snd_pcm_close(han);
//close(fa);
return NULL;
}
// Audio stream out
static gpointer audioo(gpointer p) {
usleep(1000000);
// termios setup
int fa;
unsigned char rbuf[1601];
struct termios aty;
snd_pcm_t *han;
fa = open(adev, O_WRONLY | O_NOCTTY);
if (fa < 0) cout << "Rec Audio Serial Init Error!\n" << endl; else cout << "			Record Audio Serial Ok!\n" << endl;
if (tcgetattr(fa, &aty) < 0) cout << "Error from rec audio tcgetattr\n" << endl;
aty.c_cflag = CS8 | CLOCAL;
aty.c_cflag &= ~(CREAD | CSTOPB | CRTSCTS | PARENB | HUPCL);
aty.c_iflag = IGNPAR | IGNBRK;
aty.c_iflag &= ~(BRKINT | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY | INPCK);
aty.c_oflag = 0;
aty.c_lflag = 0;
cfsetospeed(&aty, B115200);
aty.c_cc[VTIME] = 0;
aty.c_cc[VMIN] = 0;
if (tcsetattr(fa,TCSANOW,&aty) < 0) cout << "Error from rec audio tcsetattr\n" << endl;
// alsa audio setup
if (snd_pcm_open(&han, rdev, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK) < 0) cout << "			Record Audio Device Error!" << endl; else cout << "			Record Audio Device Ok!" << endl;
if (snd_pcm_set_params(han, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 1, 8000, 1, 250000) < 0) cout << "Record Params Error!" << endl;
snd_pcm_sframes_t fr;
// loop read from alsa, writes to modem
while (rst) {
if (snd_pcm_state(han) == SND_PCM_STATE_PREPARED) { snd_pcm_start(han); cout << "			rec dev restart" << endl; }
fr = snd_pcm_readi(han, rbuf, 1600);
if (fr < 0) { snd_pcm_recover(han, fr, 0); cout << "			rrec" << endl; }
if (fr < 0) { printf("			snd_pcm_readi failed: %s\n", snd_strerror(fr)); tcflush(fa, TCOFLUSH); }
else {
write(fa, rbuf, 1600);
}
usleep(99000);
}
snd_pcm_close(han);
//close(fa);
return NULL;
}
// Audio uac in
static gpointer audioiu(gpointer p) {
usleep(500000);
//loop audio from modem uac device to audio card
string alsap="alsaloop -C "s+updev+" -P "s+pdev+" -c 2 -t 200000 -f S16_LE --rate 8000"s;
system(alsap.c_str()); // execute alsaloop in another thread
return NULL;
}
// Audio uac out
static gpointer audioou(gpointer p) {
usleep(800000);
//loop audio from audio device to modem uac
string alsar="alsaloop -C "s+rdev+" -P "s+urdev+" -c 2 -t 200000 -f S16_LE --rate 8000"s;
system(alsar.c_str()); // execute alsaloop in another thread
return NULL;
}
// Start calls audio on a new thread
static void audiostart() {
if (uac) {
if (write(fd, "AT+QPCMV=1,2\r", 13) < 13) { printf("s error\n"); }; // UAC
pst=1;
rst=1;
g_thread_new("UAP", audioiu, NULL);
g_thread_new("UAR", audioou, NULL);
usleep(300000);
} else {
if (write(fd, "AT+QPCMV=1,0\r", 13) < 13) { printf("s error\n"); }; // stream
pst=1;
rst=1;
g_thread_new("PDP", audioi, NULL);
g_thread_new("RDP", audioo, NULL);
usleep(300000); }
}
// Stop calls audio
static void audiostop() {
rst=0;
pst=0;
if (uac) system("killall -s 2 alsaloop");
write(fd, "AT+QPCMV=0\r", 11);
usleep(900000);
write(fd, "AT+QPCMV=0\r", 11);
if (uac) system("killall -s 9 alsaloop");
}
// Main serial read
static string res() {
int l = 0;
string o = "";
char buf[35];
while((l = read(fd, buf, 33))) {
o.append(buf, l);
}
// output to terminal for debug
//cout << "$" << o << "|";
return o;
}
// Test the serial
static bool se() {
su=0;
if (write(fd, "AT\r", 3) < 3) { printf("test error\n"); return false; }
string o = res();
if (o.substr(0, 4) == "\r\nOK") { su=1; return true; }
return false;
}
// Check if the modem is busy
static bool ibusy() {
if (su) {
if ( cbsy || ubsy ) {
usleep(500000);
if ( cbsy || ubsy ) { sstat("Modem busy"); return true; } } }
else { sstat("Modem comm error. Press Test again."); return true; }
return false; }
// Quit
static void destroy(GtkWidget *widget, gpointer data) { gtk_main_quit(); }
// Start call
static void call() {
if (ibusy()) return;
cbsy=1;
if (cst == 0) { // call number
//get and check the number
string sn = gtk_entry_get_text(GTK_ENTRY(ennum));
int ln;
sn.erase(std::remove(sn.begin(), sn.end(), ' '), sn.end());
ln = sn.size();
if ( ln < 2 || ln > 23 || strspn(sn.c_str(),numc) < ln ) { sstat("Invalid number !"); cbsy=0; return; }
if (sn[0] == '*' || sn[0] == '#' ) { sn="AT+CUSD=1,\""+sn+"\"\r"; } // its a ussd code
else { // its a normal phone number
cout << endl << "calling: " << sn << " : " << (int)ln;
sstat("Calling: "+sn);
sn="ATD"+sn+np+";\r";
cst=1;
}
ln = sn.size();
const char *ns = sn.c_str();
//start audio and send the command
if (da) audiostart();
if (write(fd, ns, ln) < ln) printf("s error\n");
} else if (cst == 2) { // answer to call
sstat("Answering...");
if (da) audiostart();
if (write(fd, "ATA\r", 4) < 4) printf("s error\n");
cst=4;
}
cbsy=0;
return; }
// Closes call
static bool callclose() {
cbsy=1;
write(fd, "AT+CHUP\r", 8);
if (da) audiostop();
usleep(250000);
write(fd, "AT+CHUP\r", 8);
cst=0;
sstat("CALL CLOSED by you.");
cbsy=0;
return false; }
// Convert characterset from gsm to utf8
string gsmtoutf8(string o) {
int ln=o.size();
if (ln) {
int x, y;
bool fn;
for(x=0; x < ln; ++x) {
if (o[x] != 27) {
fn=0;
for (y=0; y<cs; ++y) { if (UGc[y][0]==o[x]) { fn=1; break; } }
if (fn) {
if (UGc[y][3]) { o.replace(x,1,{UGc[y][2],UGc[y][3]}); ++ln; ++x; }
else { o[x]=UGc[y][2]; } }
} else {
fn=0;
if (o[x+1] == 101) { o.replace(x,2,{226,130,172}); ++x; ++ln; //damn euro sign
} else {
for (y=0; y<cs; ++y) { if (UGc[y][0]==27 && UGc[y][1]==o[x+1]) { fn=1; break; } }
if (fn) {
if (UGc[y][3]) { o[x]=UGc[y][2]; o[x+1]=UGc[y][3]; ++x; }
else { o.replace(x,2,{UGc[y][2]}); --ln; }
} else { o[x]=96; o[x+1]=96; ++x; } } } } }
return o;
}
// Convert characterset from utf8 to gsm
string utf8togsm(string o) {
bool fn=0;
int x=0; int y=0;
int ln=o.size();
if (ln) {
for(x=0; x < ln; ++x) {
if (o[x] < 128) {
fn=0;
for (y=0; y<cs; ++y) { if (UGc[y][2]==o[x]) { fn=1; break; } }
if (fn) {
if (UGc[y][1]) { o.replace(x,1,{UGc[y][0],UGc[y][1]}); ++ln; ++x; }
else { o[x]=UGc[y][0]; } }
} else if (o[x+1] > 127) {
fn=0;
for (y=0; y<cs; ++y) { if (UGc[y][2]==o[x] && UGc[y][3]==o[x+1]) { fn=1; break; } }
if (fn) {
if (UGc[y][1]) { o[x]=UGc[y][0]; o[x+1]=UGc[y][1]; ++x; }
else { o.replace(x,2,{UGc[y][0]}); --ln; }
}
else if (o[x]==226 && o[x+1]==130 && o[x+2]==172) { o.replace(x,3,{27,101}); ++x; --ln; }
else { o[x]=96; o[x+1]=96; ++x; }
} else { o[x]=96; } } }
return o;
}
// Send a SMS
static void sendsms() {
if (ibusy()) return;
cbsy=1;
//get and check the number
string sn = gtk_entry_get_text(GTK_ENTRY(ennum));
int nl, ln;
ln = sn.size();
if ( ln < 2 || ln > 23 || strspn(sn.c_str(),numc) < ln ) { sstat("Invalid number !"); cbsy=0; return; }
sn.erase(std::remove(sn.begin(), sn.end(), ' '), sn.end());
//get and check the text
GtkTextIter be, bs;
gtk_text_buffer_get_start_iter(txtbuf, &bs);
gtk_text_buffer_get_end_iter(txtbuf, &be);
string nt = gtk_text_buffer_get_text(txtbuf, &bs, &be, true);
nl = nt.size();
while(nt[0] == ' ') { nt.erase(0,1); }
while(nt[(ln-1)] == ' ') { nt.erase(--nl,1); }
nl = nt.size();
if (!nl) { sstat("No text!"); cbsy=0; return; }
int x=0;
// detect invalid chars and characterset conversion
for(x=0; x < nl; ++x) { if (nt[x] < 32 && nt[x] != 10) { sstat("Invalid characters!"); cbsy=0; return; } }
nt = utf8togsm(nt);
nl = nt.size();
if (nl > 159) { sstat("SMS text too long !"); cbsy=0; return; }
char nm[nl+1];
strcpy(nm, nt.c_str());
nm[nl]=26;
nl++;
string oa, o;
const char esk[] = {27};
const char sub[] = {26};
bool sk = 0;
int tri  = 7;
tcflush(fd, TCIOFLUSH);
// send the first comman in text mode
sn = "AT+CMGS=\""+sn+"\"\r";
const char *nn = sn.c_str();
ln = strlen(nn);
if (write(fd, nn, ln) < ln) { printf("s error\n"); }
while (tri) { // wait for '>'
o = res();
if (o.substr(0, 3) == "\r\n>") { sk = 1; break; }
else { --tri; }
usleep(300000); }
if (sk) {
tcflush(fd, TCIOFLUSH);
cout << "ready" << endl;
// send the sub char
if (write(fd, nm, nl) < nl) { printf("s error\n"); }
usleep(1700000);
// send the text
if (write(fd, sub, 1) < 1) { printf("s error\n"); }
tri=7;
while (tri) { // wait for response
o = res();
if (o.substr(0, 8) == "\r\n+CMGS:") { sstat("SMS Sent !"); break; }
else if (o.substr(0, 12) == "\r\n+CMS ERROR") { sstat("SMS Error !"); break; }
else { sstat("SMS: no response ! "+tri);  write(fd, esk, 1); }
--tri;
usleep(250000);
} } else { sstat("SMS: modem not ready !"); write(fd, esk, 1); }
cbsy=0;
return; }
//Cellular network info
static string cnetwork() {
if (write(fd, "AT+CREG?\r", 9) < 9) { printf("s error\n"); }
string o = res();
return o; }
// Sim pin status
static string csim() {
if (write(fd, "AT+CPIN?\r", 9) < 9) { printf("s error\n"); }
string o = res();
return o;
}
// Cellular network Name
static string nname() {
if (write(fd, "AT+QSPN\r", 8) < 8) { printf("s error\n"); }
string o = res();
int pa = o.find('"');
if (pa > 0) {
int pb = o.find('"', ++pa);
o = o.substr(pa, pb-pa);
} else { o = ""; }
return o;
}
// Signal quality
static string sig() {
if (write(fd, "AT+CSQ\r", 7) < 7) { printf("s error\n"); }
string o = res();
int p = o.find(",");
o = o.substr(8, p-8);
p = strtoul(o.c_str(), NULL, 10);
if (p > 99) {
p = 100 - (p - 100); } else {
p = 100 - p; }
o = to_string(p);
return o;
}
// Cellular network status
static void test() {
cbsy=1;
tcflush(fd, TCIOFLUSH);
string o, s;
int ln = 0;
if (se()) { // modem serial test  
o.append("Serial OK");
usleep(250000);
// cellular network status
s = cnetwork();
ln = s.length();
unsigned char p = s[11];
switch(p) {
case '0':
o.append(" | Network: Unregistered"); 
break;
case '1':
o.append(" | Network: OK"); 
break;
case '2':
o.append(" | Network: Searching..."); 
break;
case '3':
o.append(" | Network: Can't register"); 
break;
case '4':
o.append(" | Network: Error"); 
break;
case '5':
o.append(" | Network: OK(roaming)"); 
break;
default :
o.append(" | Network: ?");
}
// cellular network standard
string ba="";
p = s[ln-9];
if (p > 47 && p < 56) {
p-=48;
if (p == 7) ba = "4G";
else if ( p && p < 7 ) ba = "3G";
else if (!p) ba = "2G";
}
usleep(250000);
// signal strenght
o.append(" | "+sig()+"% | "+ba);
usleep(250000);
s = nname();
if (!s.empty()) { o.append(" | "+s); }
usleep(250000);
// sim and pin status
s = csim();
ln = s.length();
o.append(" | SIM: ");
if (s.substr(0, 12) == "\r\n+CME ERROR") { o.append("No SIM!"); }
else if (s.substr(0, 14) == "\r\n+CPIN: READY") { o+="OK"; }
else if (s.substr(0, 16) == "\r\n+CPIN: SIM PIN") { o+="Insert PIN !"; }
else if (s.substr(0, 16) == "\r\n+CPIN: SIM PUK") { o+="Insert PUK !"; }
else if (s.substr(0, 17) == "\r\n+CPIN: SIM PIN2") { o+="Re insert PIN"; }
else if (s.substr(0, 17) == "\r\n+CPIN: SIM PUK2") { o+="Re insert PUK"; }
else { o+=" Error: "; o.append(s, 2, ln-3);   }
} else { o = "Serial Error !"; }
sstat(o);
cbsy=0;
}
// Settings to run at startup
static void inits() {
tcflush(fd, TCIOFLUSH);
if (se()) {
sstat("Modem Ok");
cout << "AT test ok" << endl;
usleep(250000);
write(fd, "AT+CMGF=1\r", 10); // sms text mode
usleep(250000);
write(fd, "AT+CNMI=2,2,0,0,1\r", 18); // sms setup
usleep(250000);
write(fd, "AT+QSCLK=1\r", 11); // enable auto sleep mode
usleep(250000);
write(fd, "AT+COLP=1\r", 10); // call response
usleep(250000);
write(fd, "AT+CLIP=1\r", 10); // show incoming call number
usleep(100000);
cout << "inits done" << endl;
} else { sstat("Init Serial Error"); }
tcflush(fd, TCIOFLUSH);
return; }
// Enter sim pin
static void pin() {
if (ibusy()) return;
cbsy=1;
//get and check the pin
const gchar *nn;
nn = gtk_entry_get_text(GTK_ENTRY(enpi));
string gu(nn);
unsigned int ln = gu.length();
while(gu[0] == ' ') { gu.erase(0,1); }
while(gu[(ln-1)] == ' ') { gu.erase(--ln,1); }
ln = gu.length();
if ( ln < 4 || ln > 8 || strspn(gu.c_str(),nums) < ln ) {
sstat("Invalid PIN");
cbsy=0;
return; }
//pin ok, send the command
string oc = "AT+CPIN=";
oc.append(gu);
oc+="\r";
const char *co = oc.c_str();
int nl = strlen(co);
if (write(fd, co, nl) < nl) { printf("s error\n"); }
//read the response
string o = res();
if (o.substr(0, 4) == "\r\nOK") { sstat("Pin OK !"); inits(); }
else if (o.substr(0, 15) == "\r\n+CME ERROR: 3") { sstat("Pin was ok already !"); }
else if (o.substr(0, 12) == "\r\n+CME ERROR") { sstat("Pin Error !"); }
else if (o.substr(0, 7) == "\r\nERROR") { sstat("Pin Format Error !"); }
else { sstat("Command Error !"); }
usleep(1000000);
cbsy=0;
return; 
}
// Settings to run the first time (and after a firmware update)
static void iset() {
cbsy=1;
write(fd, "ATE0\r", 5);
usleep(350000);
write(fd, "AT+QDAI=1,0,0,4,0,0,1,1\r", 24); //audio setup
usleep(350000);
write(fd, "AT+QCFG=\"USBCFG\",0x2C7C,0x0125,1,1,1,1,1,1,1\r", 45); // audio uac setup
usleep(350000);
write(fd, "AT+QGPSCFG=\"galileonmeatype\",1\r", 31); //gps,enable galileo
usleep(350000);
//write(fd, "AT+QGPSCFG=\"glonassnmeatype\",1\r", 31); //gps,enable glonass
//usleep(350000);
//write(fd, "AT+QGPSCFG=\"beidounmeatype\",1\r", 30); //gps,enable beidou
//usleep(350000);
write(fd, "AT+QGPSCFG=\"gnssconfig\",6\r", 26); //enable gps + galileo
usleep(350000);
write(fd, "AT+QGPSCFG=\"autogps\",0\r", 24); //disable gps autostart
usleep(350000);
write(fd, "AT+QLOCCFG=\"server\",\"www.gg.gg:2222\"\r", 37); //clear queclocator address
usleep(350000);
write(fd, "AT&W\r", 5); // save settings
usleep(900000);
write(fd, "AT+QPOWD\r", 9); // shut down modem
sstat("Done! Restart the modem.");
cbsy=0;
}
// Aero mode off
static void aeroff() {
cbsy=1;
if (write(fd, "AT+CFUN=1\r", 10) < 10) { printf("s error\n"); }
sstat("AERO MODE OFF!");
cbsy=0;
return;
}
// Aero mode on - cellular network off
static void aeron() {
cbsy=1;
tcflush(fd, TCIOFLUSH);
if (write(fd, "AT+CFUN=4\r", 10) < 10) { printf("s error\n"); }
string o = res();
if (o.substr(0, 4) == "\r\nOK") { sstat("AERO MODE ON!"); }
cbsy=0;
return;
}
// Get GPS position
static void gpsp() {
if (ibusy()) return;
cbsy=1;
if (write(fd, "AT+QGPSLOC?\r", 12) < 12) { printf("s error\n"); }//ask the position
string o = res();
if (o.substr(0, 12) == "\r\n+QGPSLOC: ") { sstat("Position: "+o.substr(12)); } //show the position
else if (o.substr(0, 17) == "\r\n+CME ERROR: 516") { sstat("Waiting for position..."); }
else if (o.substr(0, 17) == "\r\n+CME ERROR: 505") { sstat("GPS is off"); }
else { sstat(o); }
cbsy=0;
return;
}
// Turns GPS on
static void gpson() {
if (ibusy()) return;
cbsy=1;
if (write(fd, "AT+QGPS=1,30,999,0,1\r", 21) < 21) { printf("s error\n"); } // 999 = 50
//if (write(fd, "AT+QGPS=1\r", 10) < 10) { printf("s error\n"); } // 999 = 50
tcflush(fd, TCIOFLUSH);
cbsy=0;
return;
}
// Turns GPS off
static void gpsof() {
if (ibusy()) return;
cbsy=1;
if (write(fd, "AT+QGPSEND\r", 11) < 11) { printf("s error\n"); }
tcflush(fd, TCIOFLUSH);
cbsy=0;
return;
}
// Internet connect
static void netcon() {
if (ibusy()) return;
//execute the script to connect with qmicli
system("sudo /etc/qmi/qmic");
return;
}
// Internet disconnect
static void netdsc() {
if (ibusy()) return;
//execute the script to disconnect with qmicli
system("sudo /etc/qmi/qmid");
return;
}
// Internet status
static void netst() {
if (ibusy()) return;
cbsy=1;
sstat("Read the response in the terminal");
// internet status info from commands
printf("--- internal net status ---\n");
if (write(fd, "AT+QIACT?\r", 10) < 10) { printf("s error\n"); }
res();
usleep(500000);
printf("--- qmi net status ---\n");
if (write(fd, "AT+QNETDEVSTATUS?\r", 18) < 18) { printf("s error\n"); }
res();
cbsy=0;
//also execute the script for status info
system("sudo /etc/qmi/qmis");
return;
}
// Handle incoming messages(URC) from the modem
void ures(int s) {
if (!cbsy) {
ubsy=1;
u++;
string o = res();

if (o.substr(0, 7) == "\r\n+CMT:") { // SMS
int ln=o.size();
o = o.substr(8, ln-10);
ln=o.size();
unsigned int poa=o.find("\r\n");
if (poa < string::npos) {
string oa = o.substr(0,poa);
oa.erase(std::remove(oa.begin(), oa.end(), '"'), oa.end());
cout << "SMS:NUM:DAT::" << oa << endl;
ln=oa.size();
//parse number/date string
int nf=-1;
int z;
int poss[3];
string snum, sdat;
for(z=0; z < ln; ++z) {
if (oa[z] == ',') { ++nf;
poss[nf]=z;
if (nf > 1) break; } }
if (nf >= 0) { snum = oa.substr(0, poss[0]);
if (nf > 0) { if (nf > 1) { sdat = oa.substr(poss[1]+1, poss[2]-(poss[1]+1));
} else { sdat = oa.substr(poss[1]+1, string::npos); }
} else { sdat=" - "; } } else { snum=" - "; }
string ob = o.substr(poa+2);
ln=ob.size();
cout << "SMS:TEXT::" << ob << endl;
ob = gsmtoutf8(ob); // convert sms text charset from gsm to utf8
// show the sms in a popup dialog
string smst = "From: "+snum+"\nDate: "+sdat+"\n\n"+ob;
GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_OTHER, GTK_BUTTONS_OK, smst.c_str());
gtk_window_set_title(GTK_WINDOW(dlg), "New SMS !");
gtk_dialog_run(GTK_DIALOG(dlg));
gtk_widget_destroy(dlg);
} else { sstat("New SMS Error"); } }

else if (o.substr(0, 6) == "\r\nRING") { // call ring
cst=2;
if (o.substr(0, 15) == "\r\nRING\r\n\r\n+CLIP") { // call with caller number
unsigned int poa=o.find('"');
if (poa != string::npos) {
unsigned int pob=o.find('"', poa+1);
if (pob != string::npos && pob > poa) {
o = o.substr(poa+1, pob-poa-1);
if (o.size()) { sstat("INCOMING CALL! from "+o); } } }
} else { // just ring
sstat("INCOMING CALL!");
} }

else if (o.substr(0, 12) == "\r\nNO CARRIER") { // call closed
write(fd, "AT+CHUP\r", 8);
cst=0;
sstat("CALL CLOSED by the other side");
if (da) { audiostop(); }
}

else if (o.substr(0, 8) == "\r\n+CUSD:") { // USSD network message
unsigned int poa=o.find('"');
if (poa != string::npos) {
unsigned int pob=o.find('"', poa+1);
if (pob != string::npos && pob > poa) {
o = o.substr(poa+1, pob-poa-1);
int ln=o.size();
if (ln && ln < 183) {
o = gsmtoutf8(o);
// show dialog
GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_OTHER, GTK_BUTTONS_OK, o.c_str());
gtk_window_set_title(GTK_WINDOW(dlg), "Network Message");
gtk_dialog_run(GTK_DIALOG(dlg));
gtk_widget_destroy(dlg);
} } } }

else if (o.substr(0, 4) == "\r\nOK") {
if (cst == 1) {
cst=3; // they answered the call
sstat("Other side answered the call!");
} }

else if (o.substr(0, 6) == "\r\nBUSY") { // Busy
sstat("Busy!");
}

else if (o.substr(0, 14) == "\r\nPOWERED DOWN") {
su=0;
sstat("Modem shut down!"); // modem off
}
}
ubsy=0; }
// Shutdown modem
static void sshut() {
cbsy=1;
if (write(fd, "AT+QPOWD\r", 9) < 9) { printf("s error\n"); }
sstat("shuting down...");
su=0;
cbsy=0;
return;
}
// Main
int main( int argc, char *argv[] ) {
setbuf(stdout, NULL);
//GTK2 UI setup
GtkWidget *box;
GtkWidget *bos;
GtkWidget *lab;
GtkWidget *butt;
gtk_init(&argc, &argv);
window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);
gtk_window_set_title(GTK_WINDOW(window), "QPTest");
gtk_container_set_border_width(GTK_CONTAINER(window), 9);
gtk_widget_set_size_request(GTK_WIDGET(window), 720, 790);
box = gtk_vbox_new(false, 3);
gtk_container_add(GTK_CONTAINER(window), box);

lab = gtk_label_new("Number:");
gtk_box_pack_start(GTK_BOX(box), lab, true, true, 0);

ennum = gtk_entry_new();
gtk_entry_set_max_length(GTK_ENTRY(ennum), 23);
gtk_box_pack_start(GTK_BOX(box), ennum, true, true, 0);

lab = gtk_label_new("Messagge:");
gtk_box_pack_start(GTK_BOX(box), lab, true, true, 0);	

tvtxt = gtk_text_view_new();
txtbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tvtxt));
gtk_box_pack_start(GTK_BOX (box), tvtxt, true, true, 0);

butt = gtk_button_new_with_label("Send SMS");
g_signal_connect(butt, "clicked", G_CALLBACK(sendsms), NULL);
gtk_box_pack_start(GTK_BOX(box), butt, true, true, 5);

butt = gtk_button_new_with_label("CALL / ANSWER");
g_signal_connect(butt, "clicked", G_CALLBACK(call), NULL);
gtk_box_pack_start(GTK_BOX(box), butt, true, true, 0);

butt = gtk_button_new_with_label("Close Call");
g_signal_connect(butt, "clicked", G_CALLBACK (callclose), NULL);
gtk_box_pack_start(GTK_BOX(box), butt, true, true, 0);

bos = gtk_hbox_new(true, 3);
gtk_box_pack_start(GTK_BOX(box), bos, true, true, 0);
butt = gtk_button_new_with_label("GPS On");
g_signal_connect(butt, "clicked", G_CALLBACK(gpson), NULL);
gtk_box_pack_start(GTK_BOX(bos), butt, true, true, 0);
butt = gtk_button_new_with_label("GPS Off");
g_signal_connect(butt, "clicked", G_CALLBACK(gpsof), NULL);
gtk_box_pack_start(GTK_BOX(bos), butt, true, true, 0);
butt = gtk_button_new_with_label("Position");
g_signal_connect(butt, "clicked", G_CALLBACK(gpsp), NULL);
gtk_box_pack_start(GTK_BOX(bos), butt, true, true, 0);

bos = gtk_hbox_new (true, 3);
gtk_box_pack_start(GTK_BOX(box), bos, true, true, 0);
butt = gtk_button_new_with_label ("Connect");
g_signal_connect(butt, "clicked", G_CALLBACK(netcon), NULL);
gtk_box_pack_start(GTK_BOX(bos), butt, true, true, 0);
butt = gtk_button_new_with_label ("Disconnect");
g_signal_connect(butt, "clicked", G_CALLBACK(netdsc), NULL);
gtk_box_pack_start(GTK_BOX(bos), butt, true, true, 0);
butt = gtk_button_new_with_label ("Test Internet");
g_signal_connect(butt, "clicked", G_CALLBACK(netst), NULL);
gtk_box_pack_start(GTK_BOX(bos), butt, true, true, 0);

bos = gtk_hbox_new (true, 3);
gtk_box_pack_start(GTK_BOX(box), bos, true, true, 0);
butt = gtk_button_new_with_label ("First Run Setup");
g_signal_connect(butt, "clicked", G_CALLBACK(iset), NULL);
gtk_box_pack_start(GTK_BOX(bos), butt, true, true, 0);
butt = gtk_button_new_with_label ("Test");
g_signal_connect(butt, "clicked", G_CALLBACK(test), NULL);
gtk_box_pack_start(GTK_BOX(bos), butt, true, true, 0);
butt = gtk_button_new_with_label ("Send Pin");
g_signal_connect(butt, "clicked", G_CALLBACK(pin), NULL);
gtk_box_pack_start(GTK_BOX(bos), butt, true, true, 0);
enpi = gtk_entry_new();
gtk_entry_set_max_length (GTK_ENTRY (enpi), 8);
gtk_box_pack_start(GTK_BOX (bos), enpi, true, true, 0);

bos = gtk_hbox_new (true, 3);
gtk_box_pack_start(GTK_BOX(box), bos, true, true, 0);
butt = gtk_button_new_with_label ("Aero Mode Off");
g_signal_connect(butt, "clicked", G_CALLBACK(aeroff), NULL);
gtk_box_pack_start(GTK_BOX(bos), butt, true, true, 0);
butt = gtk_button_new_with_label ("Aero Mode On");
g_signal_connect(butt, "clicked", G_CALLBACK(aeron), NULL);
gtk_box_pack_start(GTK_BOX(bos), butt, true, true, 0);

butt = gtk_button_new_with_label ("Shut Down Modem");
g_signal_connect(butt, "clicked", G_CALLBACK (sshut), NULL);
gtk_box_pack_start(GTK_BOX(box), butt, true, true, 0);

butt = gtk_button_new_with_label ("Quit");
g_signal_connect(butt, "clicked", G_CALLBACK (destroy), NULL);
gtk_box_pack_start(GTK_BOX(box), butt, true, true, 0);

stat = gtk_statusbar_new ();      
gtk_box_pack_start(GTK_BOX(box), stat, true, true, 0);
gtk_widget_show(stat);
sc = gtk_statusbar_get_context_id(GTK_STATUSBAR(stat), "M");
gtk_statusbar_set_has_resize_grip(GTK_STATUSBAR(stat), true);

gtk_widget_show_all(window);
// Termios setup
fd = open(cdev, O_RDWR | O_NOCTTY | O_NONBLOCK);
if (fd < 0) { cout << "Serial Init Error!\n" << endl; } else { cout << "Serial Ok!\n" << endl; }
struct termios tty;
struct sigaction saio;
saio.sa_handler = ures;
sigemptyset(&(saio.sa_mask));
saio.sa_flags = 0;
saio.sa_restorer = NULL;
sigaction(SIGIO,&saio,NULL);
fcntl(fd, F_SETOWN, getpid());
fcntl(fd, F_SETFL, FASYNC);
if (tcgetattr(fd, &tty) < 0) { cout << "Error from main tcgetattr\n" << endl; }
cfsetospeed(&tty, B115200);
cfsetispeed(&tty, B115200);
tty.c_cflag |= (CLOCAL | CREAD | CS8);
tty.c_cflag &= ~(CSTOPB | CSTOPB | CRTSCTS | PARENB | HUPCL);
tty.c_iflag = IGNBRK;
tty.c_iflag &= ~(BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY | INPCK);
tty.c_lflag = 0;
tty.c_oflag = 0;
tty.c_cc[VMIN] = 0;
tty.c_cc[VTIME] = 5;
if (tcsetattr(fd, TCSANOW, &tty) != 0) { cout << "Error from main tcsetattr\n" << endl; }
usleep(300000);
inits(); //serial test and startup settings
gtk_main(); //show UI
cbsy=0;
close(fd);
return 0;
}
