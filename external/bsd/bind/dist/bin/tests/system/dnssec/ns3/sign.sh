#!/bin/sh -e
#
# Copyright (C) 2004, 2006-2012  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2000-2002  Internet Software Consortium.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
# OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

# Id: sign.sh,v 1.43 2011/11/04 05:36:28 each Exp 

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=../random.data

zone=secure.example.
infile=secure.example.db.in
zonefile=secure.example.db

cnameandkey=`$KEYGEN -T KEY -q -r $RANDFILE -a RSASHA1 -b 768 -n host cnameandkey.$zone`
dnameandkey=`$KEYGEN -T KEY -q -r $RANDFILE -a RSASHA1 -b 768 -n host dnameandkey.$zone`
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`

cat $infile $cnameandkey.key $dnameandkey.key $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

zone=bogus.example.
infile=bogus.example.db.in
zonefile=bogus.example.db

keyname=`$KEYGEN -q -r $RANDFILE -a RSAMD5 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

zone=dynamic.example.
infile=dynamic.example.db.in
zonefile=dynamic.example.db

keyname1=`$KEYGEN -q -r $RANDFILE -a RSAMD5 -b 768 -n zone $zone`
keyname2=`$KEYGEN -q -r $RANDFILE -a RSAMD5 -b 1024 -n zone -f KSK $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

zone=keyless.example.
infile=keyless.example.db.in
zonefile=keyless.example.db

keyname=`$KEYGEN -q -r $RANDFILE -a RSAMD5 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

# Change the signer field of the a.b.keyless.example SIG A
# to point to a provably nonexistent KEY record.
mv $zonefile.signed $zonefile.tmp
<$zonefile.tmp perl -p -e 's/ keyless.example/ b.keyless.example/
    if /^a.b.keyless.example/../NXT/;' >$zonefile.signed
rm -f $zonefile.tmp

#
#  NSEC3/NSEC test zone
#
zone=secure.nsec3.example.
infile=secure.nsec3.example.db.in
zonefile=secure.nsec3.example.db

keyname=`$KEYGEN -q -r $RANDFILE -a RSAMD5 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
#  NSEC3/NSEC3 test zone
#
zone=nsec3.nsec3.example.
infile=nsec3.nsec3.example.db.in
zonefile=nsec3.nsec3.example.db

keyname=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -3 - -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
#  OPTOUT/NSEC3 test zone
#
zone=optout.nsec3.example.
infile=optout.nsec3.example.db.in
zonefile=optout.nsec3.example.db

keyname=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -3 - -A -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
# A nsec3 zone (non-optout).
#
zone=nsec3.example.
infile=nsec3.example.db.in
zonefile=nsec3.example.db

keyname=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -g -3 - -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
#  OPTOUT/NSEC test zone
#
zone=secure.optout.example.
infile=secure.optout.example.db.in
zonefile=secure.optout.example.db

keyname=`$KEYGEN -q -r $RANDFILE -a RSAMD5 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
#  OPTOUT/NSEC3 test zone
#
zone=nsec3.optout.example.
infile=nsec3.optout.example.db.in
zonefile=nsec3.optout.example.db

keyname=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -3 - -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
#  OPTOUT/OPTOUT test zone
#
zone=optout.optout.example.
infile=optout.optout.example.db.in
zonefile=optout.optout.example.db

keyname=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -3 - -A -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
# A optout nsec3 zone.
#
zone=optout.example.
infile=optout.example.db.in
zonefile=optout.example.db

keyname=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -g -3 - -A -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
# A nsec3 zone (non-optout) with unknown hash algorithm.
#
zone=nsec3-unknown.example.
infile=nsec3-unknown.example.db.in
zonefile=nsec3-unknown.example.db

keyname=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -3 - -U -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
# A optout nsec3 zone.
#
zone=optout-unknown.example.
infile=optout-unknown.example.db.in
zonefile=optout-unknown.example.db

keyname=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -3 - -U -A -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
# A multiple parameter nsec3 zone.
#
zone=multiple.example.
infile=multiple.example.db.in
zonefile=multiple.example.db

keyname=`$KEYGEN -q -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1
mv $zonefile.signed $zonefile
$SIGNER -P -u3 - -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1
mv $zonefile.signed $zonefile
$SIGNER -P -u3 AAAA -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1
mv $zonefile.signed $zonefile
$SIGNER -P -u3 BBBB -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1
mv $zonefile.signed $zonefile
$SIGNER -P -u3 CCCC -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1
mv $zonefile.signed $zonefile
$SIGNER -P -u3 DDDD -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
# A RSASHA256 zone.
#
zone=rsasha256.example.
infile=rsasha256.example.db.in
zonefile=rsasha256.example.db

keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA256 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
# A RSASHA512 zone.
#
zone=rsasha512.example.
infile=rsasha512.example.db.in
zonefile=rsasha512.example.db

keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA512 -b 1024 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
# A zone with the DNSKEY set only signed by the KSK
#
zone=kskonly.example.
infile=kskonly.example.db.in
zonefile=kskonly.example.db

kskname=`$KEYGEN -q -r $RANDFILE -fk $zone`
zskname=`$KEYGEN -q -r $RANDFILE $zone`
cat $infile $kskname.key $zskname.key >$zonefile
$SIGNER -x -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
# A zone with the expired signatures
#
zone=expired.example.
infile=expired.example.db.in
zonefile=expired.example.db

kskname=`$KEYGEN -q -r $RANDFILE -fk $zone`
zskname=`$KEYGEN -q -r $RANDFILE $zone`
cat $infile $kskname.key $zskname.key >$zonefile
$SIGNER -P -r $RANDFILE -o $zone -s -1d -e +1h $zonefile > /dev/null 2>&1
rm -f $kskname.* $zskname.*

#
# A NSEC3 signed zone that will have a DNSKEY added to it via UPDATE.
#
zone=update-nsec3.example.
infile=update-nsec3.example.db.in
zonefile=update-nsec3.example.db

kskname=`$KEYGEN -q -3 -r $RANDFILE -fk $zone`
zskname=`$KEYGEN -q -3 -r $RANDFILE $zone`
cat $infile $kskname.key $zskname.key >$zonefile
$SIGNER -P -3 - -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
# A NSEC signed zone that will have auto-dnssec enabled and
# extra keys not in the initial signed zone.
#
zone=auto-nsec.example.
infile=auto-nsec.example.db.in
zonefile=auto-nsec.example.db

kskname=`$KEYGEN -q -r $RANDFILE -fk $zone`
zskname=`$KEYGEN -q -r $RANDFILE $zone`
kskname=`$KEYGEN -q -r $RANDFILE -fk $zone`
zskname=`$KEYGEN -q -r $RANDFILE $zone`
cat $infile $kskname.key $zskname.key >$zonefile
$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
# A NSEC3 signed zone that will have auto-dnssec enabled and
# extra keys not in the initial signed zone.
#
zone=auto-nsec3.example.
infile=auto-nsec3.example.db.in
zonefile=auto-nsec3.example.db

kskname=`$KEYGEN -q -3 -r $RANDFILE -fk $zone`
zskname=`$KEYGEN -q -3 -r $RANDFILE $zone`
kskname=`$KEYGEN -q -3 -r $RANDFILE -fk $zone`
zskname=`$KEYGEN -q -3 -r $RANDFILE $zone`
cat $infile $kskname.key $zskname.key >$zonefile
$SIGNER -P -3 - -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
# Secure below cname test zone.
#
zone=secure.below-cname.example.
infile=secure.below-cname.example.db.in
zonefile=secure.below-cname.example.db
keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 1024 -n zone $zone`
cat $infile $keyname.key >$zonefile
$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
# Patched TTL test zone.
#
zone=ttlpatch.example.
infile=ttlpatch.example.db.in
zonefile=ttlpatch.example.db
signedfile=ttlpatch.example.db.signed
patchedfile=ttlpatch.example.db.patched

keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`
cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -f $signedfile -o $zone $zonefile > /dev/null 2>&1
$CHECKZONE -D -s full $zone $signedfile 2> /dev/null | \
    awk '{$2 = "3600"; print}' > $patchedfile

#
# Seperate DNSSEC records.
#
zone=split-dnssec.example.
infile=split-dnssec.example.db.in
zonefile=split-dnssec.example.db
signedfile=split-dnssec.example.db.signed

keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`
cat $infile $keyname.key >$zonefile
echo '$INCLUDE "'"$signedfile"'"' >> $zonefile
: > $signedfile
$SIGNER -P -r $RANDFILE -D -o $zone $zonefile > /dev/null 2>&1

#
# Seperate DNSSEC records smart signing.
#
zone=split-smart.example.
infile=split-smart.example.db.in
zonefile=split-smart.example.db
signedfile=split-smart.example.db.signed

keyname=`$KEYGEN -q -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`
cp $infile $zonefile
echo '$INCLUDE "'"$signedfile"'"' >> $zonefile
: > $signedfile
$SIGNER -P -S -r $RANDFILE -D -o $zone $zonefile > /dev/null 2>&1

# 
# Zone with signatures about to expire, but no private key to replace them
#
zone="expiring.example."
infile="expiring.example.db.in"
zonefile="expiring.example.db"
signedfile="expiring.example.db.signed"
kskname=`$KEYGEN -q -r $RANDFILE $zone`
zskname=`$KEYGEN -q -r $RANDFILE -f KSK $zone`
cp $infile $zonefile
$SIGNER -S -r $RANDFILE -e now+1mi -o $zone $zonefile > /dev/null 2>&1
mv -f ${zskname}.private ${zskname}.private.moved
mv -f ${kskname}.private ${kskname}.private.moved

#
# A zone where the signer's name has been forced to uppercase.
#
zone="upper.example."
infile="upper.example.db.in"
zonefile="upper.example.db"
lower="upper.example.db.lower"
signedfile="upper.example.db.signed"
kskname=`$KEYGEN -q -r $RANDFILE $zone`
zskname=`$KEYGEN -q -r $RANDFILE -f KSK $zone`
cp $infile $zonefile
$SIGNER -P -S -r $RANDFILE -o $zone -f $lower $zonefile > /dev/null 2>&1
$CHECKZONE -D upper.example $lower 2>&- | \
	sed '/RRSIG/s/ upper.example. / UPPER.EXAMPLE. /' > $signedfile

#
# Check that the signer's name is in lower case when zone name is in
# upper case.
#
zone="LOWER.EXAMPLE."
infile="lower.example.db.in"
zonefile="lower.example.db"
signedfile="lower.example.db.signed"
kskname=`$KEYGEN -q -r $RANDFILE $zone`
zskname=`$KEYGEN -q -r $RANDFILE -f KSK $zone`
cp $infile $zonefile
$SIGNER -P -S -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
# Zone with signatures about to expire, and dynamic, but configured
# not to resign with 'auto-resign no;'
#
zone="nosign.example."
infile="nosign.example.db.in"
zonefile="nosign.example.db"
signedfile="nosign.example.db.signed"
kskname=`$KEYGEN -q -r $RANDFILE $zone`
zskname=`$KEYGEN -q -r $RANDFILE -f KSK $zone`
cp $infile $zonefile
$SIGNER -S -r $RANDFILE -e now+1mi -o $zone $zonefile > /dev/null 2>&1
# preserve a normalized copy of the NS RRSIG for comparison later
$CHECKZONE -D nosign.example nosign.example.db.signed 2>&- | \
        awk '$4 == "RRSIG" && $5 == "NS" {$2 = ""; print}' | \
        sed 's/[ 	][ 	]*/ /g'> ../nosign.before

#
# An inline signing zone
#
zone=inline.example.
kskname=`$KEYGEN -q -3 -r $RANDFILE -fk $zone`
zskname=`$KEYGEN -q -3 -r $RANDFILE $zone`
