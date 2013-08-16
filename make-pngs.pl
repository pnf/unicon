#!/usr/bin/perl

$ttf = "unifont-5.1.20080907.ttf";

for $i (0..5000) {
    $cmd = sprintf("./ttf2png-1.0/ttf2png -r $i,$i -s 128 -c 128 -o output/uni%05d.png $ttf",$i);
#    print "$cmd\n";
    system($cmd);
    print "." unless $i%100;
}
