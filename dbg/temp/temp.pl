#!/usr/bin/perl

my $action = shift || '';
my $file = 'FILE';
my $tmpfile = sprintf "/tmp/%s.t", $file;

if ($action eq '-in') {
    open(T, '>', $tmpfile) || die "$0: Error: $tmpfile: $!\n";
    print T scalar(localtime), "\n";
    close(T);
    exit(0);
} elsif ($action eq '-out') {
    open(T, '<', $tmpfile) || die "$0: Error: $tmpfile: $!\n";
    open(O, '>', $file) || die "$0: Error: $file: $!\n";
    print O for <T>;
    close(O);
    close(T);
    unlink $tmpfile;
    exit(0);
}

if (fork == 0) {
    exec $^X, $0, '-in';
    exit(2);
} else {
    wait;
}

# Apparently, if we fork a process to copy the temp file to the output
# then AO "works" because it still records the opening of the output
# file, and we don't really care where its input data comes from.
# But if the temp file is renamed directly
# in the parent process then the entire existence of the output file
# is forgotten because the input was ignored.

if ($action eq '-fork') {
    if (fork == 0) {
	exec $^X, $0, '-out';
	exit(2);
    } else {
	wait;
    }
} else {
    rename($tmpfile, $file) || die "$0: Error: $tmpfile => $file: $!\n";
    exit 0;
}
