# Don't edit Makefile! Use conf-* for configuration.

SHELL=/bin/sh
NROFF=nroff

default: it

.PHONY: check clean default it man test

.SUFFIXES: .0 .1 .5 .7 .8

.1.0:
	$(NROFF) -man $< >$@

.5.0:
	$(NROFF) -man $< >$@

.7.0:
	$(NROFF) -man $< >$@

.8.0:
	$(NROFF) -man $< >$@

addresses.0: \
addresses.5

auto-ccld.sh: \
conf-cc conf-ld warn-auto.sh
	( cat warn-auto.sh; \
	echo CC=\'`head -n 1 conf-cc`\'; \
	echo LD=\'`head -n 1 conf-ld`\' \
	) > auto-ccld.sh

auto-int: \
load auto-int.o
	./load auto-int -lskarnet

auto-int.o: \
compile auto-int.c
	./compile auto-int.c

auto-int8: \
load auto-int8.o
	./load auto-int8 -lskarnet

auto-int8.o: \
compile auto-int8.c
	./compile auto-int8.c

auto-str: \
load auto-str.o
	./load auto-str -lskarnet

auto-str.o: \
compile auto-str.c
	./compile auto-str.c

auto_break.c: \
auto-str conf-break
	./auto-str auto_break \
	"`head -n 1 conf-break`" > auto_break.c

auto_break.o: \
compile auto_break.c
	./compile auto_break.c

auto_patrn.c: \
auto-int8 conf-patrn
	./auto-int8 auto_patrn `head -n 1 conf-patrn` > auto_patrn.c

auto_patrn.o: \
compile auto_patrn.c
	./compile auto_patrn.c

auto_qmail.c: \
auto-str conf-qmail
	./auto-str auto_qmail `head -n 1 conf-qmail` > auto_qmail.c

auto_qmail.o: \
compile auto_qmail.c
	./compile auto_qmail.c

auto_spawn.c: \
auto-int conf-spawn
	./auto-int auto_spawn `head -n 1 conf-spawn` > auto_spawn.c

auto_spawn.o: \
compile auto_spawn.c
	./compile auto_spawn.c

auto_split.c: \
auto-int conf-split
	./auto-int auto_split `head -n 1 conf-split` > auto_split.c

auto_split.o: \
compile auto_split.c
	./compile auto_split.c

auto_usera.c: \
auto-str conf-users
	./auto-str auto_usera `head -n 1 conf-users` > auto_usera.c

auto_usera.o: \
compile auto_usera.c
	./compile auto_usera.c

auto_userd.c: \
auto-str conf-users
	./auto-str auto_userd `head -n 2 conf-users | tail -1` > auto_userd.c

auto_userd.o: \
compile auto_userd.c
	./compile auto_userd.c

auto_userl.c: \
auto-str conf-users
	./auto-str auto_userl `head -n 3 conf-users | tail -1` > auto_userl.c

auto_userl.o: \
compile auto_userl.c
	./compile auto_userl.c

auto_usero.c: \
auto-str conf-users
	./auto-str auto_usero `head -n 4 conf-users | tail -1` > auto_usero.c

auto_usero.o: \
compile auto_usero.c
	./compile auto_usero.c

auto_userp.c: \
auto-str conf-users
	./auto-str auto_userp `head -n 5 conf-users | tail -1` > auto_userp.c

auto_userp.o: \
compile auto_userp.c
	./compile auto_userp.c

auto_userq.c: \
auto-str conf-users
	./auto-str auto_userq `head -n 6 conf-users | tail -1` > auto_userq.c

auto_userq.o: \
compile auto_userq.c
	./compile auto_userq.c

auto_userr.c: \
auto-str conf-users
	./auto-str auto_userr `head -n 7 conf-users | tail -1` > auto_userr.c

auto_userr.o: \
compile auto_userr.c
	./compile auto_userr.c

auto_users.c: \
auto-str conf-users
	./auto-str auto_users `head -n 8 conf-users | tail -1` > auto_users.c

auto_users.o: \
compile auto_users.c
	./compile auto_users.c

auto_groupn.c: \
auto-str conf-groups
	./auto-str auto_groupn `head -n 2 conf-groups | tail -1` > auto_groupn.c

auto_groupn.o: \
compile auto_groupn.c
	./compile auto_groupn.c

auto_groupq.c: \
auto-str conf-groups
	./auto-str auto_groupq `head -n 1 conf-groups` > auto_groupq.c

auto_groupq.o: \
compile auto_groupq.c
	./compile auto_groupq.c

binm1: \
binm1.sh conf-qmail
	cat binm1.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	> binm1
	chmod 755 binm1

binm1+df: \
binm1+df.sh conf-qmail
	cat binm1+df.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	> binm1+df
	chmod 755 binm1+df

binm2: \
binm2.sh conf-qmail
	cat binm2.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	> binm2
	chmod 755 binm2

binm2+df: \
binm2+df.sh conf-qmail
	cat binm2+df.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	> binm2+df
	chmod 755 binm2+df

binm3: \
binm3.sh conf-qmail
	cat binm3.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	> binm3
	chmod 755 binm3

binm3+df: \
binm3+df.sh conf-qmail
	cat binm3+df.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	> binm3+df
	chmod 755 binm3+df

bouncesaying: \
load bouncesaying.o
	./load bouncesaying -lskarnet

bouncesaying.0: \
bouncesaying.1

bouncesaying.o: \
compile bouncesaying.c fork.h strerr.h error.h wait.h sig.h exit.h
	./compile bouncesaying.c

check: \
it man
	./instcheck

chkspawn: \
load chkspawn.o auto_spawn.o
	./load chkspawn auto_spawn.o -lskarnet

chkspawn.o: \
compile chkspawn.c buffer_copy.h fmt.h select.h \
auto_spawn.h
	./compile chkspawn.c

clean: \
TARGETS
	rm -f `grep -v '^#' TARGETS`
	cd tests && $(MAKE) clean

commands.o: \
compile commands.c commands.h
	./compile commands.c

compile: \
make-compile warn-auto.sh
	( cat warn-auto.sh; ./make-compile ) > compile
	chmod 755 compile

condredirect: \
load condredirect.o qmail.o buffer_copy.a auto_qmail.o
	./load condredirect qmail.o buffer_copy.a auto_qmail.o -lskarnet

condredirect.0: \
condredirect.1

condredirect.o: \
compile condredirect.c wait.h seek.h qmail.h buffer_copy.h
	./compile condredirect.c

config: \
warn-auto.sh config.sh conf-qmail conf-break conf-split
	cat warn-auto.sh config.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	| sed s}BREAK}"`head -n 1 conf-break`"}g \
	| sed s}SPLIT}"`head -n 1 conf-split`"}g \
	> config
	chmod 755 config

config-fast: \
warn-auto.sh config-fast.sh conf-qmail conf-break conf-split
	cat warn-auto.sh config-fast.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	| sed s}BREAK}"`head -n 1 conf-break`"}g \
	| sed s}SPLIT}"`head -n 1 conf-split`"}g \
	> config-fast
	chmod 755 config-fast

constmap.o: \
compile constmap.c constmap.h alloc.h case.h
	./compile constmap.c

control.o: \
compile control.c getln.h control.h
	./compile control.c

date822fmt.o: \
compile date822fmt.c fmt.h date822fmt.h
	./compile date822fmt.c

datemail: \
warn-auto.sh datemail.sh conf-qmail conf-break conf-split
	cat warn-auto.sh datemail.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	| sed s}BREAK}"`head -n 1 conf-break`"}g \
	| sed s}SPLIT}"`head -n 1 conf-split`"}g \
	> datemail
	chmod 755 datemail

direntry.h: \
compile trydrent.c direntry.h1 direntry.h2
	( ./compile trydrent.c >/dev/null 2>&1 \
	&& cat direntry.h2 || cat direntry.h1 ) > direntry.h
	rm -f trydrent.o

dot-qmail.0: \
dot-qmail.5

dot-qmail.5: \
dot-qmail.9 conf-qmail conf-break conf-spawn
	cat dot-qmail.9 \
	| sed s}QMAILHOME}"`head -n 1 conf-qmail`"}g \
	| sed s}BREAK}"`head -n 1 conf-break`"}g \
	| sed s}SPAWN}"`head -n 1 conf-spawn`"}g \
	> dot-qmail.5

env.a: \
makelib env.o envread.o
	./makelib env.a env.o envread.o

env.o: \
compile env.c str.h alloc.h env.h
	./compile env.c

envelopes.0: \
envelopes.5

envread.o: \
compile envread.c env.h str.h
	./compile envread.c

except: \
load except.o
	./load except -lskarnet

except.0: \
except.1

except.o: \
compile except.c wait.h
	./compile except.c

fifo.o: \
compile fifo.c hasmkffo.h fifo.h
	./compile fifo.c

fmtqfn.o: \
compile fmtqfn.c fmtqfn.h fmt.h auto_split.h
	./compile fmtqfn.c

forgeries.0: \
forgeries.7

forward: \
load forward.o qmail.o buffer_copy.a auto_qmail.o
	./load forward qmail.o buffer_copy.a auto_qmail.o -lskarnet

forward.0: \
forward.1

forward.o: \
compile forward.c sig.h qmail.h \
	./compile forward.c

fs.a: \
makelib fmt_str.o fmt_strn.o
	./makelib fs.a fmt_str.o fmt_strn.o

getln.a: \
makelib getln.o
	./makelib getln.a getln.o

getln.o: \
compile getln.c getln.h
	./compile getln.c

gfrom.o: \
compile gfrom.c str.h gfrom.h
	./compile gfrom.c

gid.o: \
compile gid.c uidgid.h
	./compile gid.c

hasflock.h: \
tryflock.c compile load
	( ( ./compile tryflock.c && ./load tryflock ) >/dev/null \
	2>&1 \
	&& echo \#define HASFLOCK 1 || exit 0 ) > hasflock.h
	rm -f tryflock.o tryflock

hasmkffo.h: \
trymkffo.c compile load
	( ( ./compile trymkffo.c && ./load trymkffo ) >/dev/null \
	2>&1 \
	&& echo \#define HASMKFIFO 1 || exit 0 ) > hasmkffo.h
	rm -f trymkffo.o trymkffo

hasnpbg1.h: \
trynpbg1.c compile load
	( ( ./compile trynpbg1.c \
	&& ./load trynpbg1 -lskarnet && ./trynpbg1 ) \
	>/dev/null 2>&1 \
	&& echo \#define HASNAMEDPIPEBUG1 1 || exit 0 ) > \
	hasnpbg1.h
	rm -f trynpbg1.o trynpbg1

hassalen.h: \
trysalen.c compile
	( ./compile trysalen.c >/dev/null 2>&1 \
	&& echo \#define HASSALEN 1 || exit 0 ) > hassalen.h
	rm -f trysalen.o

hassgact.h: \
trysgact.c compile load
	( ( ./compile trysgact.c && ./load trysgact ) >/dev/null \
	2>&1 \
	&& echo \#define HASSIGACTION 1 || exit 0 ) > hassgact.h
	rm -f trysgact.o trysgact

hassgprm.h: \
trysgprm.c compile load
	( ( ./compile trysgprm.c && ./load trysgprm ) >/dev/null \
	2>&1 \
	&& echo \#define HASSIGPROCMASK 1 || exit 0 ) > hassgprm.h
	rm -f trysgprm.o trysgprm

haswaitp.h: \
trywaitp.c compile load
	( ( ./compile trywaitp.c && ./load trywaitp ) >/dev/null \
	2>&1 \
	&& echo \#define HASWAITPID 1 || exit 0 ) > haswaitp.h
	rm -f trywaitp.o trywaitp

headerbody.o: \
compile headerbody.c getln.h hfield.h headerbody.h
	./compile headerbody.c

hfield.o: \
compile hfield.c hfield.h
	./compile hfield.c

hier.o: \
compile hier.c auto_qmail.h auto_split.h auto_uids.h fmt.h hier.h
	./compile hier.c

home: \
home.sh conf-qmail
	cat home.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	> home
	chmod 755 home

home+df: \
home+df.sh conf-qmail
	cat home+df.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	> home+df
	chmod 755 home+df

hostname: \
load hostname.o dns.lib socket.lib
	./load hostname -lskarnet `cat dns.lib` \
	`cat socket.lib`

hostname.o: \
compile hostname.c
	./compile hostname.c

install: \
instpackage instchown warn-auto.sh
	( cat warn-auto.sh; echo './instpackage && ./instchown' ) > install
	chmod 755 install

instcheck: \
load instcheck.o instuidgid.o hier.o auto_qmail.o auto_split.o \
ids.a fs.a
	./load instcheck instuidgid.o hier.o auto_qmail.o auto_split.o \
	ids.a fs.a -lskarnet

instcheck.o: \
compile instcheck.c hier.h
	./compile instcheck.c

instchown: \
load instchown.o instuidgid.o hier.o auto_qmail.o auto_split.o \
ids.a fs.a
	./load instchown instuidgid.o hier.o auto_qmail.o auto_split.o \
	ids.a fs.a -lskarnet

instchown.o: \
compile instchown.c hier.h
	./compile instchown.c

instfiles.o: \
compile instfiles.c
	./compile instfiles.c

instpackage: \
load instpackage.o instfiles.o hier.o auto_qmail.o auto_split.o \
buffer_copy.a fs.a 
	./load instpackage instfiles.o hier.o auto_qmail.o auto_split.o \
	buffer_copy.a fs.a -lskarnet

instpackage.o: \
compile instpackage.c open.h strerr.h hier.h
	./compile instpackage.c

instqueue: \
load instqueue.o instfiles.o hier.o auto_qmail.o auto_split.o \
buffer_copy.a fs.a
	./load instqueue instfiles.o hier.o auto_qmail.o auto_split.o \
	buffer_copy.a fs.a -lskarnet

instqueue.o: \
compile instqueue.c open.h strerr.h hier.h
	./compile instqueue.c

instuidgid.o: \
compile instuidgid.c uidgid.h auto_uids.h auto_users.h
	./compile instuidgid.c

ipme.o: \
compile ipme.c hassalen.h byte.h ip.h ipalloc.h ip.h gen_alloc.h \
stralloc.h gen_alloc.h ipme.h ip.h ipalloc.h
	./compile ipme.c

ipmeprint: \
load ipmeprint.o ipme.o socket.lib
	./load ipmeprint ipme.o `cat socket.lib` -lskarnet

ipmeprint.o: \
compile ipmeprint.c ipme.h ipalloc.h
	./compile ipmeprint.c

it: \
qmail-local qmail-lspawn qmail-getpw qmail-remote qmail-rspawn \
qmail-clean qmail-send qmail-start splogger qmail-queue qmail-inject \
predate datemail mailsubj qmail-upq qmail-showctl qmail-newu \
qmail-pw2u qmail-qread qmail-qstat qmail-tcpto qmail-tcpok \
qmail-pop3d qmail-popup qmail-qmqpc qmail-qmqpd qmail-qmtpd \
qmail-smtpd sendmail qmail-newmrh config config-fast \
ipmeprint qreceipt qbiff \
forward preline condredirect bouncesaying except maildirmake \
maildir2mbox install instpackage instqueue instchown \
instcheck home home+df proc proc+df binm1 binm1+df binm2 binm2+df \
binm3 binm3+df

load: \
make-load warn-auto.sh
	( cat warn-auto.sh; ./make-load ) > load
	chmod 755 load

lock.a: \
makelib lock_ex.o lock_exnb.o lock_un.o
	./makelib lock.a lock_ex.o lock_exnb.o lock_un.o

lock_ex.o: \
compile lock_ex.c hasflock.h lock.h
	./compile lock_ex.c

lock_exnb.o: \
compile lock_exnb.c hasflock.h lock.h
	./compile lock_exnb.c

lock_un.o: \
compile lock_un.c hasflock.h lock.h
	./compile lock_un.c

maildir.0: \
maildir.5

maildir.o: \
compile maildir.c prioq.h maildir.h strerr.h
	./compile maildir.c

maildir2mbox: \
load maildir2mbox.o maildir.o prioq.o myctime.o gfrom.o \
getln.a strerr.a fs.a buffer_copy.a
	./load maildir2mbox maildir.o prioq.o myctime.o \
	gfrom.o getln.a strerr.a fs.a buffer_copy.a -lskarnet

maildir2mbox.0: \
maildir2mbox.1

maildir2mbox.o: \
compile maildir2mbox.c prioq.h buffer_copy.h getln.h \
gfrom.h myctime.h maildir.h strerr.h
	./compile maildir2mbox.c

maildirmake: \
load maildirmake.o
	./load maildirmake -lskarnet

maildirmake.0: \
maildirmake.1

maildirmake.o: \
compile maildirmake.c strerr.h
	./compile maildirmake.c

mailsubj: \
warn-auto.sh mailsubj.sh conf-qmail conf-break conf-split
	cat warn-auto.sh mailsubj.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	| sed s}BREAK}"`head -n 1 conf-break`"}g \
	| sed s}SPLIT}"`head -n 1 conf-split`"}g \
	> mailsubj
	chmod 755 mailsubj

mailsubj.0: \
mailsubj.1

make-compile: \
make-compile.sh auto-ccld.sh
	cat auto-ccld.sh make-compile.sh > make-compile
	chmod 755 make-compile

make-load: \
make-load.sh auto-ccld.sh
	cat auto-ccld.sh make-load.sh > make-load
	chmod 755 make-load

make-makelib: \
make-makelib.sh auto-ccld.sh
	cat auto-ccld.sh make-makelib.sh > make-makelib
	chmod 755 make-makelib

makelib: \
make-makelib warn-auto.sh
	( cat warn-auto.sh; ./make-makelib ) > \
	makelib
	chmod 755 makelib

man: \
qmail-local.0 qmail-lspawn.0 qmail-getpw.0 qmail-remote.0 \
qmail-rspawn.0 qmail-clean.0 qmail-send.0 qmail-start.0 splogger.0 \
qmail-queue.0 qmail-inject.0 mailsubj.0 qmail-showctl.0 qmail-newu.0 \
qmail-pw2u.0 qmail-qread.0 qmail-qstat.0 qmail-tcpto.0 qmail-tcpok.0 \
qmail-pop3d.0 qmail-popup.0 qmail-qmqpc.0 qmail-qmqpd.0 qmail-qmtpd.0 \
qmail-smtpd.0 tcp-env.0 qmail-newmrh.0 qreceipt.0 qbiff.0 forward.0 \
preline.0 condredirect.0 bouncesaying.0 except.0 maildirmake.0 \
maildir2mbox.0 qmail.0 qmail-limits.0 qmail-log.0 \
qmail-control.0 qmail-header.0 qmail-users.0 dot-qmail.0 \
qmail-command.0 tcp-environ.0 maildir.0 mbox.0 addresses.0 \
envelopes.0 forgeries.0

mbox.0: \
mbox.5

myctime.o: \
compile myctime.c fmt.h myctime.h
	./compile myctime.c

newfield.o: \
compile newfield.c fmt.h date822fmt.h newfield.h
	./compile newfield.c

oflops.h: \
chkbiofl.c compile load oflops_bi.h oflops_compat.h
	 ( ( ./compile chkbiofl.c  && ./load chkbiofl && \
	./chkbiofl ) >/dev/null 2>&1 \
	&& cat oflops_bi.h || cat oflops_compat.h ) > oflops.h
	rm -f chkbiofl.o chkbiofl

package: \
it man
	./instpackage

predate: \
load predate.o strerr.a buffer_copy.a
	./load predate strerr.a buffer_copy.a -lskarnet

predate.o: \
compile predate.c wait.h fmt.h strerr.h buffer_copy.h
	./compile predate.c

preline: \
load preline.o buffer_copy.a
	./load preline buffer_copy.a -lskarnet

preline.0: \
preline.1

preline.o: \
compile preline.c buffer_copy.h wait.h
	./compile preline.c

prioq.o: \
compile prioq.c prioq.h
	./compile prioq.c

proc: \
proc.sh conf-qmail
	cat proc.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	> proc
	chmod 755 proc

proc+df: \
proc+df.sh conf-qmail
	cat proc+df.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	> proc+df
	chmod 755 proc+df

prot.o: \
compile prot.c prot.h
	./compile prot.c

qbiff: \
load qbiff.o headerbody.o hfield.o getln.a
	./load qbiff headerbody.o hfield.o getln.a -lskarnet

qbiff.0: \
qbiff.1

qbiff.o: \
compile qbiff.c headerbody.h hfield.h qtmp.h
	./compile qbiff.c

qmail-clean: \
load qmail-clean.o fmtqfn.o getln.a fs.a auto_qmail.o auto_split.o
	./load qmail-clean fmtqfn.o getln.a fs.a auto_qmail.o \
	auto_split.o -lskarnet

qmail-clean.0: \
qmail-clean.8

qmail-clean.o: \
compile qmail-clean.c getln.h fmtqfn.h auto_qmail.h
	./compile qmail-clean.c

qmail-command.0: \
qmail-command.8

qmail-control.0: \
qmail-control.5

qmail-control.5: \
qmail-control.9 conf-qmail conf-break conf-spawn
	cat qmail-control.9 \
	| sed s}QMAILHOME}"`head -n 1 conf-qmail`"}g \
	| sed s}BREAK}"`head -n 1 conf-break`"}g \
	| sed s}SPAWN}"`head -n 1 conf-spawn`"}g \
	> qmail-control.5

qmail-getpw: \
load qmail-getpw.o auto_break.o ids.a
	./load qmail-getpw auto_break.o ids.a -lskarnet

qmail-getpw.0: \
qmail-getpw.8

qmail-getpw.8: \
qmail-getpw.9 conf-qmail conf-break conf-spawn
	cat qmail-getpw.9 \
	| sed s}QMAILHOME}"`head -n 1 conf-qmail`"}g \
	| sed s}BREAK}"`head -n 1 conf-break`"}g \
	| sed s}SPAWN}"`head -n 1 conf-spawn`"}g \
	> qmail-getpw.8

qmail-getpw.o: \
compile qmail-getpw.c auto_users.h auto_break.h qlx.h
	./compile qmail-getpw.c

qmail-header.0: \
qmail-header.5

qmail-inject: \
load qmail-inject.o headerbody.o hfield.o newfield.o quote.o \
control.o date822fmt.o constmap.o qmail.o token822.o getln.a \
fs.a auto_qmail.o
	./load qmail-inject headerbody.o hfield.o newfield.o \
	quote.o control.o date822fmt.o constmap.o qmail.o \
	getln.a token822.o fs.a auto_qmail.o -lskarnet

qmail-inject.0: \
qmail-inject.8

qmail-inject.o: \
compile qmail-inject.c getln.h fmt.h hfield.h token822.h control.h \
qmail.h quote.h headerbody.h auto_qmail.h newfield.h \
constmap.h oflops.h
	./compile qmail-inject.c

qmail-limits.0: \
qmail-limits.7

qmail-limits.7: \
qmail-limits.9 conf-qmail conf-break conf-spawn
	cat qmail-limits.9 \
	| sed s}QMAILHOME}"`head -n 1 conf-qmail`"}g \
	| sed s}BREAK}"`head -n 1 conf-break`"}g \
	| sed s}SPAWN}"`head -n 1 conf-spawn`"}g \
	> qmail-limits.7

qmail-local: \
load qmail-local.o qmail.o quote.o gfrom.o myctime.o \
slurpclose.o getln.a lock.a fs.a buffer_copy.a \
auto_qmail.o auto_patrn.o socket.lib
	./load qmail-local qmail.o quote.o gfrom.o myctime.o \
	slurpclose.o getln.a lock.a fs.a buffer_copy.a \
	auto_qmail.o auto_patrn.o  `cat socket.lib` -lskarnet

qmail-local.0: \
qmail-local.8

qmail-local.o: \
compile qmail-local.c lock.h getln.h fmt.h \
buffer_copy.h quote.h qmail.h slurpclose.h myctime.h gfrom.h \
auto_patrn.h
	./compile qmail-local.c

qmail-log.0: \
qmail-log.5

qmail-lspawn: \
load qmail-lspawn.o spawn.o prot.o slurpclose.o \
ids.a fs.a auto_qmail.o auto_spawn.o
	./load qmail-lspawn spawn.o prot.o slurpclose.o \
	auto_qmail.o auto_spawn.o ids.a fs.a -lskarnet

qmail-lspawn.0: \
qmail-lspawn.8

qmail-lspawn.o: \
compile qmail-lspawn.c prot.h wait.h slurpclose.h uidgid.h \
auto_qmail.h auto_uids.h auto_users.h qlx.h spawn.h
	./compile qmail-lspawn.c

qmail-newmrh: \
load qmail-newmrh.o getln.a auto_qmail.o
	./load qmail-newmrh getln.a auto_qmail.o -lskarnet

qmail-newmrh.0: \
qmail-newmrh.8

qmail-newmrh.8: \
qmail-newmrh.9 conf-qmail conf-break conf-spawn
	cat qmail-newmrh.9 \
	| sed s}QMAILHOME}"`head -n 1 conf-qmail`"}g \
	| sed s}BREAK}"`head -n 1 conf-break`"}g \
	| sed s}SPAWN}"`head -n 1 conf-spawn`"}g \
	> qmail-newmrh.8

qmail-newmrh.o: \
compile qmail-newmrh.c getln.h auto_qmail.h \
	./compile qmail-newmrh.c

qmail-newu: \
load qmail-newu.o getln.a auto_qmail.o
	./load qmail-newu getln.a auto_qmail.o -lskarnet

qmail-newu.0: \
qmail-newu.8

qmail-newu.8: \
qmail-newu.9 conf-qmail conf-break conf-spawn
	cat qmail-newu.9 \
	| sed s}QMAILHOME}"`head -n 1 conf-qmail`"}g \
	| sed s}BREAK}"`head -n 1 conf-break`"}g \
	| sed s}SPAWN}"`head -n 1 conf-spawn`"}g \
	> qmail-newu.8

qmail-newu.o: \
compile qmail-newu.c getln.h auto_qmail.h
	./compile qmail-newu.c

qmail-pop3d: \
load qmail-pop3d.o commands.o maildir.o prioq.o getln.a strerr.a \
socket.lib
	./load qmail-pop3d commands.o maildir.o prioq.o getln.a strerr.a \
	`cat socket.lib` -lskarnet

qmail-pop3d.0: \
qmail-pop3d.8

qmail-pop3d.o: \
compile qmail-pop3d.c commands.h getln.h prioq.h maildir.h
	./compile qmail-pop3d.c

qmail-popup: \
load qmail-popup.o commands.o socket.lib
	./load qmail-popup commands.o `cat socket.lib` -lskarnet

qmail-popup.0: \
qmail-popup.8

qmail-popup.o: \
compile qmail-popup.c commands.h wait.h
	./compile qmail-popup.c

qmail-pw2u: \
load qmail-pw2u.o constmap.o control.o getln.a \
ids.a auto_break.o auto_qmail.o
	./load qmail-pw2u constmap.o control.o getln.a \
	ids.a auto_break.o auto_qmail.o -lskarnet

qmail-pw2u.0: \
qmail-pw2u.8

qmail-pw2u.8: \
qmail-pw2u.9 conf-qmail conf-break conf-spawn
	cat qmail-pw2u.9 \
	| sed s}QMAILHOME}"`head -n 1 conf-qmail`"}g \
	| sed s}BREAK}"`head -n 1 conf-break`"}g \
	| sed s}SPAWN}"`head -n 1 conf-spawn`"}g \
	> qmail-pw2u.8

qmail-pw2u.o: \
compile qmail-pw2u.c control.h constmap.h getln.h auto_break.h \
auto_qmail.h auto_users.h
	./compile qmail-pw2u.c

qmail-qmqpc: \
load qmail-qmqpc.o slurpclose.o control.o auto_qmail.o \
getln.a fs.a socket.lib
	./load qmail-qmqpc slurpclose.o control.o auto_qmail.o \
	getln.a -lskarnet `cat socket.lib`

qmail-qmqpc.0: \
qmail-qmqpc.8

qmail-qmqpc.o: \
compile qmail-qmqpc.c getln.h slurpclose.h auto_qmail.h control.h
	./compile qmail-qmqpc.c

qmail-qmqpd: \
load qmail-qmqpd.o received.o date822fmt.o qmail.o auto_qmail.o \
fs.a
	./load qmail-qmqpd received.o date822fmt.o qmail.o \
	auto_qmail.o fs.a -lskarnet

qmail-qmqpd.0: \
qmail-qmqpd.8

qmail-qmqpd.o: \
compile qmail-qmqpd.c auto_qmail.h qmail.h now.h datetime.h fmt.h
	./compile qmail-qmqpd.c

qmail-qmtpd: \
load qmail-qmtpd.o rcpthosts.o control.o constmap.o received.o \
date822fmt.o qmail.o getln.a fs.a auto_qmail.o
	./load qmail-qmtpd rcpthosts.o control.o constmap.o \
	received.o date822fmt.o qmail.o getln.a \
	fs.a auto_qmail.o -lskarnet

qmail-qmtpd.0: \
qmail-qmtpd.8

qmail-qmtpd.o: \
compile qmail-qmtpd.c qmail.h now.h datetime.h fmt.h rcpthosts.h \
auto_qmail.h control.h received.h
	./compile qmail-qmtpd.c

qmail-qread: \
load qmail-qread.o fmtqfn.o readsubdir.o date822fmt.o \
getln.a fs.a auto_qmail.o auto_split.o
	./load qmail-qread fmtqfn.o readsubdir.o date822fmt.o \
	getln.a fs.a auto_qmail.o auto_split.o -lskarnet

qmail-qread.0: \
qmail-qread.8

qmail-qread.o: \
compile qmail-qread.c fmt.h getln.h fmtqfn.h readsubdir.h \
auto_qmail.h date822fmt.h
	./compile qmail-qread.c

qmail-qstat: \
warn-auto.sh qmail-qstat.sh conf-qmail
	cat warn-auto.sh qmail-qstat.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	> qmail-qstat
	chmod 755 qmail-qstat

qmail-qstat.0: \
qmail-qstat.8

qmail-queue: \
load qmail-queue.o triggerpull.o fmtqfn.o date822fmt.o \
buffer_copy.a fs.a auto_qmail.o \
auto_split.o ids.a
	./load qmail-queue triggerpull.o fmtqfn.o \
	date822fmt.o auto_qmail.o \
	auto_split.o ids.a buffer_copy.a fs.a -lskarnet

qmail-queue.0: \
qmail-queue.8

qmail-queue.o: \
compile qmail-queue.c fmt.h \
buffer_copy.h triggerpull.h extra.h uidgid.h auto_qmail.h \
auto_uids.h auto_users.h date822fmt.h fmtqfn.h
	./compile qmail-queue.c

qmail-remote: \
load qmail-remote.o control.o constmap.o tcpto.o ipme.o \
quote.o lock.a getln.a auto_qmail.o socket.lib
	./load qmail-remote control.o constmap.o tcpto.o \
	ipme.o quote.o lock.a getln.a auto_qmail.o \
	-ls6dns -lskarnet `cat socket.lib`

qmail-remote.0: \
qmail-remote.8

qmail-remote.o: \
compile qmail-remote.c auto_qmail.h ipalloc.h tcpto.h oflops.h
	./compile qmail-remote.c

qmail-rspawn: \
load qmail-rspawn.o spawn.o tcpto_clean.o \
lock.a auto_qmail.o auto_spawn.o ids.a
	./load qmail-rspawn spawn.o tcpto_clean.o \
	lock.a auto_qmail.o ids.a auto_spawn.o -lskarnet

qmail-rspawn.0: \
qmail-rspawn.8

qmail-rspawn.o: \
compile qmail-rspawn.c wait.h tcpto.h spawn.h
	./compile qmail-rspawn.c

qmail-send: \
load qmail-send.o qsutil.o control.o constmap.o newfield.o prioq.o \
trigger.o fmtqfn.o quote.o readsubdir.o qmail.o date822fmt.o \
getln.a lock.a fs.a auto_qmail.o auto_split.o
	./load qmail-send qsutil.o control.o constmap.o newfield.o \
	prioq.o trigger.o fmtqfn.o quote.o readsubdir.o qmail.o \
	date822fmt.o getln.a lock.a fs.a auto_qmail.o auto_split.o \
	-lskarnet

qmail-send.0: \
qmail-send.8

qmail-send.8: \
qmail-send.9 conf-qmail conf-break conf-spawn
	cat qmail-send.9 \
	| sed s}QMAILHOME}"`head -n 1 conf-qmail`"}g \
	| sed s}BREAK}"`head -n 1 conf-break`"}g \
	| sed s}SPAWN}"`head -n 1 conf-spawn`"}g \
	> qmail-send.8

qmail-send.o: \
compile qmail-send.c control.h lock.h getln.h \
auto_qmail.h trigger.h newfield.h quote.h \
qmail.h qsutil.h prioq.h constmap.h fmtqfn.h readsubdir.h
	./compile qmail-send.c

qmail-send.service: \
qmail-send.service.in conf-qmail
	cat qmail-send.service.in \
	| sed s}QMAILHOME}"`head -n 1 conf-qmail`"}g \
	> qmail-send.service

qmail-showctl: \
load qmail-showctl.o control.o getln.a \
ids.a auto_qmail.o auto_break.o auto_patrn.o \
auto_spawn.o auto_split.o
	./load qmail-showctl control.o getln.a ids.a auto_qmail.o \
	auto_break.o auto_patrn.o auto_spawn.o auto_split.o -lskarnet

qmail-showctl.0: \
qmail-showctl.8

qmail-showctl.o: \
compile qmail-showctl.c control.h constmap.h uidgid.h \
auto_uids.h auto_users.h auto_qmail.h auto_break.h auto_patrn.h \
auto_spawn.h auto_split.h
	./compile qmail-showctl.c

qmail-smtpd: \
load qmail-smtpd.o rcpthosts.o commands.o ipme.o \
control.o constmap.o received.o date822fmt.o qmail.o \
getln.a fs.a auto_qmail.o socket.lib
	./load qmail-smtpd rcpthosts.o commands.o ipme.o \
	control.o constmap.o received.o date822fmt.o qmail.o \
	getln.a fs.a auto_qmail.o `cat socket.lib` -lskarnet

qmail-smtpd.0: \
qmail-smtpd.8

qmail-smtpd.o: \
compile qmail-smtpd.c \
auto_qmail.h control.h received.h constmap.h \
ipme.h qmail.h rcpthosts.h commands.h
	./compile qmail-smtpd.c

qmail-start: \
load qmail-start.o prot.o ids.a
	./load qmail-start prot.o ids.a -lskarnet

qmail-start.0: \
qmail-start.8

qmail-start.8: \
qmail-start.9 conf-qmail conf-break conf-spawn
	cat qmail-start.9 \
	| sed s}QMAILHOME}"`head -n 1 conf-qmail`"}g \
	| sed s}BREAK}"`head -n 1 conf-break`"}g \
	| sed s}SPAWN}"`head -n 1 conf-spawn`"}g \
	> qmail-start.8

qmail-start.o: \
compile qmail-start.c prot.h uidgid.h auto_uids.h auto_users.h
	./compile qmail-start.c

qmail-tcpok: \
load qmail-tcpok.o lock.a auto_qmail.o
	./load qmail-tcpok lock.a auto_qmail.o -lskarnet

qmail-tcpok.0: \
qmail-tcpok.8

qmail-tcpok.o: \
compile qmail-tcpok.c lock.h auto_qmail.h
	./compile qmail-tcpok.c

qmail-tcpto: \
load qmail-tcpto.o lock.a auto_qmail.o
	./load qmail-tcpto lock.a auto_qmail.o -lskarnet

qmail-tcpto.0: \
qmail-tcpto.8

qmail-tcpto.o: \
compile qmail-tcpto.c subfd.h buffer_copy.h auto_qmail.h byte.h \
fmt.h ip.h lock.h error.h exit.h open.h
	./compile qmail-tcpto.c

qmail-upq: \
warn-auto.sh qmail-upq.sh conf-qmail conf-break conf-split
	cat warn-auto.sh qmail-upq.sh \
	| sed s}QMAIL}"`head -n 1 conf-qmail`"}g \
	| sed s}BREAK}"`head -n 1 conf-break`"}g \
	| sed s}SPLIT}"`head -n 1 conf-split`"}g \
	> qmail-upq
	chmod 755 qmail-upq

qmail-users.0: \
qmail-users.5

qmail-users.5: \
qmail-users.9 conf-qmail conf-break conf-spawn
	cat qmail-users.9 \
	| sed s}QMAILHOME}"`head -n 1 conf-qmail`"}g \
	| sed s}BREAK}"`head -n 1 conf-break`"}g \
	| sed s}SPAWN}"`head -n 1 conf-spawn`"}g \
	> qmail-users.5

qmail.0: \
qmail.7

qmail.o: \
compile qmail.c buffer_copy.h readwrite.h wait.h exit.h fork.h fd.h \
qmail.h auto_qmail.h env.h
	./compile qmail.c

qreceipt: \
load qreceipt.o headerbody.o hfield.o quote.o token822.o qmail.o \
getln.a auto_qmail.o
	./load qreceipt headerbody.o hfield.o quote.o token822.o \
	qmail.o getln.a auto_qmail.o -lskarnet

qreceipt.0: \
qreceipt.1

qreceipt.o: \
compile qreceipt.c getln.h hfield.h token822.h headerbody.h \
quote.h qmail.h oflops.h
	./compile qreceipt.c

qsutil.o: \
compile qsutil.c qsutil.h
	./compile qsutil.c

qtmp.h: \
tryutmpx.c compile load qtmp.h1 qtmp.h2
	( ( ./compile tryutmpx.c && ./load tryutmpx ) >/dev/null 2>&1 \
	&& cat qtmp.h2 || cat qtmp.h1 ) > qtmp.h
	rm -f tryutmpx.o tryutmpx

quote.o: \
compile quote.c oflops.h
	./compile quote.c

rcpthosts.o: \
compile rcpthosts.c control.h constmap.h rcpthosts.h 
	./compile rcpthosts.c

readsubdir.o: \
compile readsubdir.c readsubdir.h fmt.h auto_split.h
	./compile readsubdir.c

received.o: \
compile received.c qmail.h date822fmt.h received.h
	./compile received.c

remoteinfo.o: \
compile remoteinfo.c fmt.h remoteinfo.h
	./compile remoteinfo.c

select.h: \
compile trysysel.c select.h1 select.h2
	( ./compile trysysel.c >/dev/null 2>&1 \
	&& cat select.h2 || cat select.h1 ) > select.h
	rm -f trysysel.o trysysel

sendmail: \
load sendmail.o auto_qmail.o
	./load sendmail auto_qmail.o -lskarnet

sendmail.o: \
compile sendmail.c auto_qmail.h
	./compile sendmail.c

setup: \
it man
	./instpackage
	./instchown

slurpclose.o: \
compile slurpclose.c slurpclose.h
	./compile slurpclose.c

socket.lib: \
trylsock.c compile load
	( ( ./compile trylsock.c && \
	./load trylsock -lsocket -lnsl ) >/dev/null 2>&1 \
	&& echo -lsocket -lnsl || exit 0 ) > socket.lib
	rm -f trylsock.o trylsock

spawn.o: \
compile chkspawn spawn.c auto_qmail.h auto_uids.h auto_spawn.h spawn.h
	./chkspawn
	./compile spawn.c

splogger: \
load splogger.o syslog.lib socket.lib
	./load splogger `cat syslog.lib` `cat socket.lib` -lskarnet

splogger.0: \
splogger.8

splogger.o: \
compile splogger.c
	./compile splogger.c

strerr.a: \
makelib strerr_sys.o strerr_die.o
	./makelib strerr.a strerr_sys.o strerr_die.o

strerr_die.o: \
compile strerr_die.c strerr.h
	./compile strerr_die.c

strerr_sys.o: \
compile strerr_sys.c strerr.h
	./compile strerr_sys.c

buffer_copy.a: \
makelib buffer_copy.o
	./makelib buffer_copy.a buffer_copy.o

buffer_copy.o: \
compile buffer_copy.c buffer_copy.h
	./compile buffer_copy.c

syslog.lib: \
trysyslog.c compile load
	( ( ./compile trysyslog.c && \
	./load trysyslog -lgen ) >/dev/null 2>&1 \
	&& echo -lgen || exit 0 ) > syslog.lib
	rm -f trysyslog.o trysyslog

tcp-env: \
load tcp-env.o dns.o remoteinfo.o timeoutread.o timeoutwrite.o \
timeoutconn.o ip.o ipalloc.o case.a ndelay.a sig.a env.a getopt.a \
stralloc.a substdio.a error.a str.a fs.a dns.lib socket.lib
	./load tcp-env dns.o remoteinfo.o timeoutread.o \
	timeoutwrite.o timeoutconn.o ip.o ipalloc.o case.a ndelay.a \
	sig.a env.a getopt.a stralloc.a substdio.a error.a \
	str.a fs.a  `cat dns.lib` `cat socket.lib`

tcp-env.0: \
tcp-env.1

tcp-env.o: \
compile tcp-env.c sig.h stralloc.h gen_alloc.h str.h env.h fmt.h \
scan.h subgetopt.h ip.h dns.h byte.h remoteinfo.h exit.h case.h
	./compile tcp-env.c

tcp-environ.0: \
tcp-environ.5

tcpto.o: \
compile tcpto.c tcpto.h lock.h now.h datetime.h datetime.h
	./compile tcpto.c

tcpto_clean.o: \
compile tcpto_clean.c tcpto.h
	./compile tcpto_clean.c

test: it
	@cd tests && $(MAKE) test

token822.o: \
compile token822.c token822.h oflops.h
	./compile token822.c

trigger.o: \
compile trigger.c trigger.h hasnpbg1.h
	./compile trigger.c

triggerpull.o: \
compile triggerpull.c triggerpull.h
	./compile triggerpull.c

uid.o: \
compile uid.c uidgid.h
	./compile uid.c

ids.a: \
makelib auto_usera.o auto_userd.o auto_userl.o auto_usero.o auto_userp.o \
auto_userq.o auto_userr.o auto_users.o auto_groupn.o auto_groupq.o gid.o uid.o
	./makelib ids.a auto_usera.o auto_userd.o auto_userl.o auto_usero.o \
	auto_userp.o auto_userq.o auto_userr.o auto_users.o auto_groupn.o \
	auto_groupq.o gid.o uid.o
