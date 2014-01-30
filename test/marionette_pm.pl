#!/usr/bin/perl 

use Test::More tests => 11;

use lib '../clients/perl/usr/local/share/perl5';
use Marionette;

my ($_m, $_ecd);

# object instantiate
$_m = Marionette->new(DEBUG => 0);
ok(1, ref($_m));

# asking ruok
$_ecd = $_m->execute("ruok");
ok ($_ecd == 0, "[ruok] successful");

# asking ruok1
$_ecd = $_m->execute("ruok1");
ok ($_ecd != 0, "[ruok1] got error, successful");

# asking test.echo
$_ecd = $_m->execute("test.echo", "-n", "foo");
ok ($_ecd == 0, "[test.echo] successful");
ok ($_m->{result} == 'foo', "[test.echo == foo] successful");

# asking test.noecho
$_ecd = $_m->execute("test.noecho", "-n", "foo");
ok ($_ecd == 100, "[test.noecho] successful");

# asking true
$_ecd = $_m->execute("true");
ok ($_ecd == 0, "[true] successful");
ok ($_m->{result} == '', "[true == ''] successful");

# asking false
$_ecd = $_m->execute("false");
ok ($_ecd == 103, "[false] successful");

# dangerous arg with test.echo
$_ecd = $_m->execute("test.echo", "-n", "foo;bar");
ok ($_ecd == 100, "[test.echo dangerous] successful");

# wrong arg_count with test.echo
$_ecd = $_m->execute("test.echo", "-n", "foo", "bar");
ok ($_ecd == 100, "[test.echo wrong arg_count] successful");
