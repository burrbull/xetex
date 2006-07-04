#!/usr/bin/perl

# /****************************************************************************\
#  Part of the XeTeX typesetting system
#  copyright (c) 1994-2005 by SIL International
#  written by Jonathan Kew
# 
#  This software is distributed under the terms of the Common Public License,
#  version 1.0.
#  For details, see <http://www.opensource.org/licenses/cpl1.0.php> or the file
#  cpl1.0.txt included with the software.
# \****************************************************************************/

# script to create unicode-letters.tex by processing UnicodeData.txt

while (<>) {
	chomp;
	@u = split(/;/);
	last if length($u[0]) > 4;
	if ($u[1] =~ /First>/) {
		$start = hex "0x$u[0]";
		$_ = <>;
		chomp;
		@u = split(/;/);
		if ($u[1] =~ /Last>/) {
			$end = hex "0x$u[0]";
			if ($u[2] =~ /^L/) {
				foreach ($start .. $end) {
					push(@letters, sprintf("%04X", $_));
				}
			}
		}
	}
	else {
		$lccode{$u[0]} = $u[13] if $u[13] ne '' and length($u[13]) <= 4;
		$lccode{$u[0]} = $u[0]  if $u[13] eq '' and $u[2] =~ /^L/;
		$uccode{$u[0]} = $u[12] if $u[12] ne '' and length($u[12]) <= 4;
		$uccode{$u[0]} = $u[0]  if $u[12] eq '' and $u[2] =~ /^L/;
		if ($u[2] =~ /^L/) {
			push(@letters, $u[0]);
		}
		elsif ($u[2] =~ /^M/) {
			push(@marks, $u[0]);
		}
		elsif (exists $lccode{$u[0]} or exists $uccode{$u[0]}) {
			push(@casesym, $u[0]);
		}
	}
}

$date = `date`;
chomp $date;
print << "__EOT__";
% Do not edit this file!
% Created from UnicodeData.txt by unicode-char-prep.pl on $date.
% In case of errors, fix the Perl script instead.
__EOT__

print << '__EOT__';
\begingroup
\catcode`\{=1 \catcode`\}=2 \catcode`\#=6
\def\C #1 #2 #3 {\global\uccode"#1="#2 \global\lccode"#1="#3 } % case mappings (non-letter)
\def\L #1 #2 #3 {\global\catcode"#1=11 \C #1 #2 #3 % letter with case mappings
  \global\XeTeXextmathcode"#1="7"01"#1 } % letters default to class 7 (var), fam 1
\def\l #1 {\L #1 #1 #1 } % letter without case mappings
\let\m=\l % combining mark - treated as uncased letter
__EOT__
for (@letters) {
	if (exists $uccode{$_} or exists $lccode{$_}) {
		if (($lccode{$_} eq $_) and ($uccode{$_} eq $_)) {
			print "\\l $_\n";
		}
		else {
			print "\\L $_ ";
			print exists $uccode{$_} ? $uccode{$_} : "0";
			print " ";
			print exists $lccode{$_} ? $lccode{$_} : "0";
			print "\n";
		}
	}
	else {
		print "\\l $_\n";
	}
}
for (@casesym) {
	print "\\C $_ ";
	print exists $uccode{$_} ? $uccode{$_} : "0";
	print " ";
	print exists $lccode{$_} ? $lccode{$_} : "0";
	print "\n";
}
for (@marks) {
	print "\\m $_\n";
}
print << '__EOT__';
\endgroup
__EOT__
