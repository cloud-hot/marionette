# Author : Vigith Maurice
# Purpose: Interface for Marionette TCP Server

package Marionette;

use Time::HiRes qw/time sleep/;
use strict;
use IO::Socket::INET;
use Carp;

use constant {
  CTRL_A => chr(1),		# arg sep
  CTRL_E => chr(5),		# command sep

  # status codes
  START_DONUT_BAKING => "200",	# started the command 
  END_DONUT_BAKING   => "201",	# ended successfully
  NO_DONUT_BAKING    => "100",	# execute failed, could be any reason (EXEC didn't happen) 
  NO_DONUT_FOR_YOU   => "101",  # no command found
  ERR_DONUT_EXEC     => "102",  # execvp failed
  ERR_DONUT_BAKING   => "103",  # exec command failed
      
};

our $err_table = {
		  "200" => "START_DONUT_BAKING",
		  "201" => "END_DONUT_BAKING",
		  "100" => "NO_DONUT_BAKING",
		  "101" => "NO_DONUT_FOR_YOU",
		  "102" => "ERR_DONUT_EXEC",
		  "103" => "ERR_DONUT_BAKING",
		 };

sub new {
  my $class = shift;

  ## the class will be instantiated with options passed as key value pairs
  my $self  = { map { uc($_) => {@_}->{$_} } keys %{ {@_} } };
  $self->{DEBUG} ||= 0;		  # turn it off, unless mentioned
  $self->{HOST}  ||= 'localhost'; # default host is localhost
  $self->{PORT}  ||= '9990';	  # default port 9990
  $self->{TIMEOUT} ||= 900;	  # default timeout is 900

  my $socket = new IO::Socket::INET (
				     PeerHost => $self->{HOST},
				     PeerPort => $self->{PORT},
				     Timeout  => $self->{TIMEOUT},
				     Proto => 'tcp',
				    ) or confess "ERROR in Socket Creation : $!\n";

  $self->{sock} = $socket;	# save the socket

  bless $self, $class;  

  $self->_reset_result();	# lets init it

  return $self
}

# reset the command vars
sub _reset_result {
  my $self = shift;

  $self->{errmsg}  = '';
  $self->{errcode} = 0;
  $self->{result}  = '';
  $self->{runtime} = -1;

  return
}

sub DESTROY {
  my $self = shift;
  # automatically closes the file if it's open (IO::Handle doc)
  undef $self->{sock};		# this will close the socket 
}

sub _execute {
  my $self = shift;
  my $cmd = shift;		# this can never be empty

  my $recv = undef;		# recv buffer
  my $end = 0;			# should we end recv loop

  my $data = '';		# data we collected
  my $ecd = 0;			# are we seeing error (err code)
  my $emg = '';			# are we seeing error (err msg)

  # send the request
  my $_s = $self->{sock}->send($cmd); 
  if ($_s == 0) {
    confess "Socket Connection Broke in [send]"
  }

  # recv
  $self->{sock}->recv($recv, 3); # get start padding message
  if (length($recv) <= 0) {
    confess "Socket Connection Broke in [receive]"
  }

  # keep receiving till we reach the end
  while (not $end) {
    $ecd = _continue($recv, \$data, \$end, \$emg); # process
    if (not $end) {			   # have we reached till the end
      $self->{sock}->recv($recv, 512); # 
      if (length($recv) <= 0) {
	confess "Socket Connection Broke in [receive]"
      }
    } else {
      last
    }
  }
  
  $self->{errmsg}  = $emg;
  $self->{errcode} = $ecd;
  $self->{result}  = substr($data, 0, -3); # remove the end pad
  $self->{runtime} = time() - $self->{runtime};

  return $ecd
}


sub _continue {
  my ($recv, $data, $end, $emg) = @_;

  $$data .= $recv;		  # there is a chance that we got last 3 chars in 2 flights
  my $e_pad = substr($$data, -3); # take the last 3 chars
  my $ecd = 0;
  if ($e_pad eq "200") {
    $$end = 0;
    $$emg = '';
    $$data = '';		# unset data, 200 is for starting
  } elsif ($e_pad eq "201") {
    $$end = 1;
    $$emg = '';
  } elsif ($e_pad eq "100" || $e_pad eq "101" || $e_pad eq "102" || $e_pad eq "103") {
    $$end = 1;
    $$emg = $err_table->{$recv};
    $ecd  = int($recv);
  } else {
    $$end = 0;
  }

  return $ecd
}

#########################
# User Exposed Function #
#########################

sub execute {
  my $self = shift;
  my $command = shift;
  my (@args,) = (@_,);

  croak "Expected Atleast 1 Argument 'command name', additional arguments will be taken as marionette args" 
    if not $command;

  # command[^e] || command[^e]arg1[^a]arg2[^a]arg3[^e]
  my $cmd = $command.CTRL_E;
  my $arg = join CTRL_A, @args;
  
  $cmd .= $arg.CTRL_E if $arg;

  print "DEBUG> [Command: $cmd]\n" if $self->{DEBUG};

  # reset the vars
  $self->_reset_result();	# to clean up from prev request

  # set the start time
  $self->{runtime} = time();

  # execute the command
  return $self->_execute($cmd);
}

1;

=pod

=head1 Marionette

Perl Interface for Marionette Server.

Marionette is a perl interface to connect to the marionette server. Once you have
created an object, you can call the execute function to execute the remote command.

=head1 SYNOPSIS

  use Marionette;

  my $_m = Marionette->new(
       HOST => "127.0.0.1",	# default is 127.0.0.1
       PORT => 9990,		# default is 9990
       TIMEOUT => 60,		# default is 900
       DEBUG => 0);

  my $err_code = $_m->execute("test.echo", "-n", "foo");

  # when object gets out of scope DESTORY will do a socket close
  # so we don't have to worry about closing the socket.

=head1 Functions

=head2 execute

The only exposed function is 'execute'. It executes the command on the marionette server (remote host). For each execute, following data is set in the object as per the status of previous
execute.

  my $err_code = $obj->execute('command_name', args....);

  $obj->{errmsg}  = '';		# err message from server
  $obj->{errcode} = 0;		# err code from server
  $obj->{result}  = '';		# result from remote execution
  $obj->{runtime} = -1;		# time for execute

=head3 return value

The error code is as follows. Error Message will be the string of contant values

    START_DONUT_BAKING = "200"  # started the command
    END_DONUT_BAKING   = "201"  # ended successfully
    NO_DONUT_BAKING    = "100"  # execute failed, could be any reason (EXEC didn't happen)
    NO_DONUT_FOR_YOU   = "101"  # no command found
    ERR_DONUT_EXEC     = "102"  # execvp failed
    ERR_DONUT_BAKING   = "103"  # exec command failed


=head1 Miscellaneous

=head2 Bugs

Probably Many!

=head2 TODO

Use B<pack> instead of string parsing

=head2 Authors

Vigith Maurice E<lt>vigith@sharethis.comE<gt>


=cut
