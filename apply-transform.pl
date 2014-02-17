#!/usr/bin/perl
#
# (c) 2014 Przemyslaw Iskra <sparky@pld-linux.org>
# You can use this code under the terms of AGPL v3 license.
#
use strict;
use warnings;

my $state_file = shift @ARGV;
my $suffix = pop @ARGV;
my $dir = pop @ARGV;
$dir .= '/' if length $dir;


my $state;
my @args;
{
	open my $f_in, '<', $state_file or die;
	local $/ = undef;
	my $VAR1;
	$state = eval <$f_in>;
	@args = split /\s+/, $state->{cmdline};
}

my @files;
my @args_files;
foreach my $file ( @ARGV )
{
	local $_ = $file;
	s/(\.png)$/$suffix-tmp$1/;
	s#^(.*/)?#$dir#;
	my $tmp = $_;
	s/-tmp\.png$/\.jpg/;
	push @files, {
		input => $file,
		tmp => $tmp,
		output => $_,
	};
	push @args_files, $file, $tmp;
}

while ( my @af = splice @args_files, 0, 200 )
{
	warn "Command: @args @af\n";
	#system "pexec", "add",
	system @args, @af;
}

#system "pexec", "wait";

foreach my $file ( @files )
{
	#system "pexec", "add",
	system "convert", "-verbose",
		( $state->{file_real} || $state->{file} ), $file->{tmp},
		"-compose", "Multiply", "-composite",
		"-scale", "1024x1024",
		$file->{output};
}
#system "pexec", "wait";

foreach my $file ( @files )
{
	unlink $file->{tmp};
}
