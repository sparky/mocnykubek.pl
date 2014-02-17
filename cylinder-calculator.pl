#!/usr/bin/perl
#
# (c) 2014 Przemyslaw Iskra <sparky@pld-linux.org>
# You can use this code under the terms of AGPL license.
#
use strict;
use warnings;

use SDL;
use SDL::Cursor;
use SDL::Mouse;
use SDL::Event;
use SDL::Events;
use SDLx::App;
use Data::Dumper;

my $file = shift @ARGV;
my @points;

die "File not given" unless -r $file;
if ( $file =~ /\.state/ )
{
	open my $f_in, '<', $file
		or die;
	local $/ = undef;
	my $VAR1;
	my $state = eval <$f_in>;

	$file = $state->{file};
	@points = @{ $state->{points} };
}

my $app = SDLx::App->new(
	width => 1000,
	height => 1000,
	resizeable => 1,
	title => 'Policz cylindra',
	depth => 32,
);

my @data = (
	0b00001000,0b00000000,
	0b00001000,0b00000000,
	0b00001000,0b00000000,
	0b00001000,0b00000000,
	0b11111111,0b10000000,
	0b00001000,0b00000000,
	0b00001000,0b00000000,
	0b00001000,0b00000000,
	0b00001000,0b00000000,
);
my @mask = @data;

my $cursor = SDL::Cursor->new( \@data, \@mask, 9, 9, 4, 4 );
SDL::Mouse::set_cursor( $cursor );


$app->add_event_handler( \&any_event );
$app->add_show_handler( \&show );

my $image_move_x = 0;
my $image_move_y = 0;

my $image = SDLx::Surface->load( $file )
	or die "Cannot load image $file\n";

my $color = {
	cross => [ 0, 0, 255, 80 ],
	point => [ 255, 255, 0, 140 ],
	grid => [ 40, 40, 40, 180 ],
	focus => [ 255, 0, 0, 140 ],
	red => [ 255, 0, 0, 80 ],
	bokeh_ball => [ 255, 0, 0, 255 ],
};

my $mouse_x = 0;
my $mouse_y = 0;
my $need_update = 1;
my $move_update;
show();
$app->run;

sub any_event
{
	my $event = shift;
	my $controller = shift;
	my $t = $event->type;
	if ( $t == SDL_MOUSEMOTION )
	{
		$mouse_x = $event->motion_x;
		$mouse_y = $event->motion_y;
		$need_update = 1;
		if ( $move_update )
		{
			no strict 'refs';
			$move_update->( $mouse_x, $mouse_y );
		}
	}
	elsif ( $t == SDL_MOUSEBUTTONDOWN or $t == SDL_MOUSEBUTTONUP )
	{
		my $x = $event->button_x;
		my $y = $event->button_y;
		my $button = $event->button_button();
		my $do = $t == SDL_MOUSEBUTTONDOWN ? 'start' : 'stop';
		my $func;
		if ( $button == SDL_BUTTON_LEFT )
		{
			$func = "point";
		}
		elsif ( $button == SDL_BUTTON_RIGHT )
		{
			$func = "scroll";
		}
		if ( $func )
		{
			$func .= "_$do";
			no strict 'refs';
			$func->( $x, $y );
		}
	}
	elsif ( $t == SDL_KEYDOWN )
	{
		my $sym = $event->key_sym;
		if ( $sym == SDLK_ESCAPE or $sym == SDLK_q )
		{
			exit;
		}
		elsif ( $sym == SDLK_c )
		{
			print cmdline() . "\n";
		}
		elsif ( $sym == SDLK_s )
		{
			save();
		}
	}
	elsif ( $t == SDL_VIDEORESIZE )
	{
		$app->resize( $event->resize_w, $event->resize_h );
		scroll_start( 0, 0 );
		scroll_stop( 0, 0 );
		$need_update = 1;
	}
	elsif ( $t == SDL_QUIT )
	{
		warn "Good bye!\n";
		$controller->stop;
	}
}

my $point_index;
sub point_start
{
	my ( $new_x, $new_y ) = @_;
	$new_x += $image_move_x;
	$new_y += $image_move_y;

	$point_index = undef;

	my $i = 0;
	foreach my $p ( @points )
	{
		my ( $x, $y ) = @$p;
		my $diff_x = $x - $new_x;
		my $diff_y = $y - $new_y;
		if ( $diff_x * $diff_x + $diff_y * $diff_y < 20 )
		{
			$point_index = $i;
			last;
		}
		$i++;
	}

	unless ( defined $point_index )
	{
		if ( @points > 11 )
		{
			warn "No more points needed\n";
			return;
		}
		push @points, [ $new_x, $new_y ];
		$point_index = $#points;
	}
	$need_update = 1;
	$move_update = "point_update";
}

sub point_update
{
	my ( $x, $y ) = @_;
	return unless $move_update and $move_update eq "point_update";
	$points[ $point_index ]->[ 0 ] = $x + $image_move_x;
	$points[ $point_index ]->[ 1 ] = $y + $image_move_y;
	$need_update = 1;
}

sub point_stop
{
	point_update( @_ );
	$move_update = undef;
}

my ( $scroll_x, $scroll_y );
sub scroll_start
{
	( $scroll_x, $scroll_y ) = @_;
	$move_update = "scroll_update";
}

sub scroll_update
{
	my ( $x, $y ) = @_;
	my $max_x = $image->w - $app->w;
	my $max_y = $image->h - $app->h;
	my $diff_x = $x - $scroll_x;
	my $diff_y = $y - $scroll_y;

	$image_move_x -= $diff_x;
	if ( $image_move_x < 0 )
	{
		$image_move_x = 0;
	}
	elsif ( $image_move_x > $max_x )
	{
		$image_move_x = $max_x;
	}

	$image_move_y -= $diff_y;
	if ( $image_move_y < 0 )
	{
		$image_move_y = 0;
	}
	elsif ( $image_move_y > $max_y )
	{
		$image_move_y = $max_y;
	}

	$scroll_x = $x;
	$scroll_y = $y;
}

sub scroll_stop
{
	scroll_update( @_ );
	$move_update = undef;
	$scroll_x = undef;
	$scroll_y = undef;
}

sub draw_point
{
	my ( $p, $c ) = @_;
	return unless $p;
	my ( $x, $y ) = @$p;
	$x -= $image_move_x;
	$y -= $image_move_y;
	my $diff_x = $x - $mouse_x;
	my $diff_y = $y - $mouse_y;
	$c //= 'point';
	$c = $color->{ $c };
	if ( $diff_x * $diff_x + $diff_y * $diff_y < 10 )
	{
		$app->draw_circle( [ $x, $y ], 4, $c );
	}
	$app->draw_line( [$x, $y - 10], [$x, $y + 10], $c );
	$app->draw_line( [$x - 10, $y], [$x + 10, $y], $c );
}

sub draw_line
{
	my ( $p1, $p2, $cname ) = @_;
	return unless $p2;

	$cname ||= 'point';
	my ( $x1, $y1 ) = @$p1;
	my ( $x2, $y2 ) = @$p2;
	$x1 -= $image_move_x;
	$y1 -= $image_move_y;
	$x2 -= $image_move_x;
	$y2 -= $image_move_y;
	$app->draw_line( [$x1, $y1], [$x2, $y2], $color->{ $cname } );
}

sub point_middle
{
	my ( $ax, $ay ) = @{ shift() };
	my ( $bx, $by ) = @{ shift() };
	my $p = [
		( $ax + $bx ) / 2,
		( $ay + $by ) / 2,
	];
	#warn "Middle: $p->[0], $p->[1]\n";
	return $p;
}

my ( $angle_first, $angle_last );
my ( $bokeh_r1, $bokeh_r2 );
sub draw_points
{
	draw_point( $points[ 0 ] );
	draw_point( $points[ 1 ] );
	draw_line( $points[ 0 ], $points[ 1 ] );
	return if @points < 2;
	draw_point( $points[ 2 ] );
	draw_point( $points[ 3 ] );
	draw_line( $points[ 2 ], $points[ 3 ] );
	return if @points < 4;
	my $m1 = point_middle( @points[ 0, 2 ] );
	my $m2 = point_middle( @points[ 1, 3 ] );

	return if @points < 5;
	draw_point( $points[ 4 ] );
	( $angle_first, $angle_last, my $starts )
		= make_elipse( $m1, $points[ 4 ], $points[ 0 ], $points[ 6 ], $points[ 7 ] );
	draw_point( $points[ 6 ] );
	draw_point( $points[ 7 ] );
	return if @points < 6;
	draw_point( $points[ 5 ] );
	my $stops = make_elipse2( $m2, $points[ 5 ], $points[ 1 ], $angle_first, $angle_last );

	my $int = find_intersection( $points[ 0 ], $points[ 1 ], $points[ 2 ], $points[ 3 ] );

	return unless $int;
	my @centers = find_hiperbola( $int, $m1, $m2 );
	my @through = find_hiperbola( $int, $points[ 4 ], $points[ 5 ] );
	my @sides = find_hiperbola( $int, $points[ 0 ], $points[ 1 ] );

	foreach my $i (  1..7  )
	{
		make_elipse( $centers[ $i ], $through[ $i ], $sides[ $i ] );
	}

	if ( $starts and $stops )
	{
		foreach my $i ( 0..8 )
		{
			draw_line( $starts->[ $i ], $stops->[ $i ], 'grid' );
		}
	}

	return if @points < 9;
	draw_point( $points[ 8 ], 'focus' );
	return if @points < 10;
	draw_point( $points[ 9 ], 'focus' );
	return if @points < 11;
	draw_point( $points[ 10 ], 'focus' );
	$bokeh_r1 = make_elipse3( @points[ 8, 9, 10 ], 2.5 );
	return if @points < 12;
	draw_point( $points[ 11 ], 'focus' );
	$bokeh_r2 = make_elipse3( @points[ 8, 9, 11 ], 5 );
}

sub find_intersection
{
	my ( $A, $B, $C, $D ) = @_;
	my ( $a, $b ) = @$A; # point a.x, a.y
	my ( $c, $d ) = @$C; # point c.x, c.y
	my ( $e, $f ) = ( $a - $B->[0], $b - $B->[1] ); # vector v1.x, v1.y
	my ( $g, $h ) = ( $c - $D->[0], $d - $D->[1] ); # vector v2.x, v2.y

	my $down = $e * $h - $f * $g;
	unless ( $down )
	{
		warn "Lines are parallel\n";
		return;
	}

	my $k = $h * ( $c - $a ) + $g * ( $b - $d );
	$k /= $down;

	my $i = $a + $k * $e;
	my $j = $b + $k * $f;

	my $p = [ $i, $j ];
	return $p;
}

sub find_hiperbola
{
	my ( $origin, $A, $B ) = @_;

	my $d1 = dist( $origin, $A );
	my $d2 = dist( $origin, $B );

	my $h = 8 * $d2 / ( $d2 - $d1 );
	my $m = - $h * $d1;

	my $vx = $A->[0] - $origin->[0];
	my $vy = $A->[1] - $origin->[1];

	my @out;
	foreach my $x ( 0..8 )
	{
		my $y = $m / ( $x - $h );
		my $Vx = $vx * $y / $d1;
		my $Vy = $vy * $y / $d1;
		my $px = $origin->[0] + $Vx;
		my $py = $origin->[1] + $Vy;
		push @out, [ $px, $py ];
	}
	return @out;
}

sub divide_line
{
	my $parts = shift;
	my $pa = shift;
	my $pb = shift;
	my ( $ax, $ay ) = @$pa;
	my ( $bx, $by ) = @$pb;
	my $diffx = $bx - $ax;
	my $diffy = $by - $ay;
	$diffx /= $parts;
	$diffy /= $parts;
	my @out;
	foreach my $i ( 0..$parts )
	{
		push @out, [ $ax, $ay ];
		$ax += $diffx;
		$ay += $diffy;
	}
	return @out;
}

sub show
{
	return unless $need_update;
	$image->blit( $app, [ $image_move_x, $image_move_y, $app->w, $app->h] );
	draw_points();
	$app->draw_line( [$mouse_x, 0], [$mouse_x, $app->h], $color->{cross} );
	$app->draw_line( [0, $mouse_y], [$app->w, $mouse_y], $color->{cross} );
	$app->update;
	$need_update = 0;
}

sub dist
{
	my ( $ax, $ay ) = @{ shift() };
	my ( $bx, $by ) = @{ shift() };
	my $x = $bx - $ax;
	my $y = $by - $ay;
	return sqrt( $x * $x + $y * $y );
}

use constant
	pi => 2 * atan2 1, 0;

sub make_elipse
{
	my ( $center, $down, $side, @close ) = @_;
	my $r1 = dist( $center, $side );
	my $dist = dist( $center, $down );
	my $angle = atan2 $side->[1] - $center->[1], $side->[0] - $center->[0];
	my $angle_down = atan2 $down->[1] - $center->[1], $down->[0] - $center->[0];

	my $sinbeta = sin $angle;
	my $cosbeta = cos $angle;

	my $r2;
	{
		my $alpha = - $angle + $angle_down;
		return unless $alpha;
		my $sinalpha = sin $alpha;
		my $cosalpha = cos $alpha;
		my $under = $r1 ** 2 - ( $dist * $cosalpha ) ** 2;
		return if $under <= 0;
		$r2 = ( $dist * $r1 * $sinalpha ) / sqrt( $under );
	}

	my ( $lastx, $lasty );
	@close = grep { defined } @close;
	my $points = 240;
	$points = 960 if @close;
	my @closest_dist = ( 1000 ) x @close;
	my @closest_point = ( undef ) x @close;
	my @closest_angle = ( undef ) x @close;
	foreach my $i ( 0..$points )
	{
		my $alpha = $i / $points * pi;
		my $sinalpha = sin $alpha;
		my $cosalpha = cos $alpha;
		my $x = $center->[0] + $r1 * $cosalpha * $cosbeta - $r2 * $sinalpha * $sinbeta;
		my $y = $center->[1] + $r1 * $cosalpha * $sinbeta + $r2 * $sinalpha * $cosbeta;
		if ( @close )
		{
			foreach my $i ( 0 .. $#close )
			{
				my $dist = dist( $close[ $i ], [ $x, $y ] );
				if ( $dist < $closest_dist[ $i ] )
				{
					$closest_point[ $i ] = [ $x, $y ];
					$closest_dist[ $i ] = $dist;
					$closest_angle[ $i ] = $alpha;
				}
			}
		}
		$x -= $image_move_x;
		$y -= $image_move_y;
		$app->draw_line( [$lastx, $lasty], [$x, $y], $color->{grid} )
			if $lastx or $lasty;
		$lastx = $x;
		$lasty = $y;
	}

	foreach my $point ( @closest_point )
	{
		draw_point( $point, 'red' );
	}

	return unless @closest_angle >= 2;

	my ( $first, $last, undef ) = @closest_angle;
	my $diff = $last - $first;
	$diff /= 8;
	my @out;
	foreach my $i ( 0..8 )
	{
		my $alpha = $first + $i * $diff;
		my $sinalpha = sin $alpha;
		my $cosalpha = cos $alpha;
		my $x = $center->[0] + $r1 * $cosalpha * $cosbeta - $r2 * $sinalpha * $sinbeta;
		my $y = $center->[1] + $r1 * $cosalpha * $sinbeta + $r2 * $sinalpha * $cosbeta;
		push @out, [ $x, $y ];
	}
	return $first, $last, \@out;
}

sub make_elipse2
{
	my ( $center, $down, $side, $first, $last ) = @_;
	my $r1 = dist( $center, $side );
	my $dist = dist( $center, $down );
	my $angle = atan2 $side->[1] - $center->[1], $side->[0] - $center->[0];
	my $angle_down = atan2 $down->[1] - $center->[1], $down->[0] - $center->[0];

	my $sinbeta = sin $angle;
	my $cosbeta = cos $angle;

	my $r2;
	{
		my $alpha = - $angle + $angle_down;
		return unless $alpha;
		my $sinalpha = sin $alpha;
		my $cosalpha = cos $alpha;
		my $under = $r1 ** 2 - ( $dist * $cosalpha ) ** 2;
		return if $under <= 0;
		$r2 = ( $dist * $r1 * $sinalpha ) / sqrt( $under );
	}

	my ( $lastx, $lasty );
	my $points = 240;
	foreach my $i ( 0..$points )
	{
		my $alpha = $i / $points * pi;
		my $sinalpha = sin $alpha;
		my $cosalpha = cos $alpha;
		my $x = $center->[0] + $r1 * $cosalpha * $cosbeta - $r2 * $sinalpha * $sinbeta;
		my $y = $center->[1] + $r1 * $cosalpha * $sinbeta + $r2 * $sinalpha * $cosbeta;
		$x -= $image_move_x;
		$y -= $image_move_y;
		$app->draw_line( [$lastx, $lasty], [$x, $y], $color->{grid} )
			if $lastx or $lasty;
		$lastx = $x;
		$lasty = $y;
	}

	return unless $first and $last;

	my $diff = $last - $first;
	$diff /= 8;
	my @out;
	foreach my $i ( 0..8 )
	{
		my $alpha = $first + $i * $diff;
		my $sinalpha = sin $alpha;
		my $cosalpha = cos $alpha;
		my $x = $center->[0] + $r1 * $cosalpha * $cosbeta - $r2 * $sinalpha * $sinbeta;
		my $y = $center->[1] + $r1 * $cosalpha * $sinbeta + $r2 * $sinalpha * $cosbeta;
		push @out, [ $x, $y ];
	}
	return \@out;
}

sub make_elipse3
{
	my ( $f1, $f2, $side, $bokeh_r ) = @_;
	my $center = [ ( $f1->[0] + $f2->[0] ) / 2,
		( $f1->[1] + $f2->[1] ) / 2 ];
	my $k = dist( $f1, $side ) + dist( $f2, $side );
	my $r1 = $k / 2;
	my $dc = dist( $f1, $center );
	my $r2 = sqrt( $r1 * $r1 - $dc * $dc );
	my $angle = atan2 $f1->[1] - $center->[1], $f1->[0] - $center->[0];

	my $sinbeta = sin $angle;
	my $cosbeta = cos $angle;

	my ( $lastx, $lasty );
	my $points = 240;
	foreach my $i ( 0..$points )
	{
		my $alpha = $i / $points * pi * 2;
		my $sinalpha = sin $alpha;
		my $cosalpha = cos $alpha;
		my $x = $center->[0] + $r1 * $cosalpha * $cosbeta - $r2 * $sinalpha * $sinbeta;
		my $y = $center->[1] + $r1 * $cosalpha * $sinbeta + $r2 * $sinalpha * $cosbeta;
		$x -= $image_move_x;
		$y -= $image_move_y;
		$app->draw_line( [$lastx, $lasty], [$x, $y], $color->{focus} )
			if $lastx or $lasty;
		$lastx = $x;
		$lasty = $y;
	}
	my ( $x, $y ) = @$side;
	$x -= $image_move_x;
	$y -= $image_move_y;
	$app->draw_circle( [ $x, $y ], $bokeh_r, $color->{bokeh_ball} );

	return $k;
}



sub cmdline
{
	my @data = (
		$image->w(), $image->h(),
		945, 1063,
		( map { ( $_->[0], $_->[1] ) } @points[ 0..5 ] ),
		$angle_first, $angle_last,
		( map { ( $_->[0], $_->[1] ) } @points[ 8..9 ] ),
		$bokeh_r1, $bokeh_r2,
	);
	return "./bender @data";
}



sub save
{
	my $i = 0;
	my $sfile;
	do
	{
		$i++;
		$sfile = $file . "-$i.state";
	} while ( -r $sfile );
	warn "Saving state to $sfile\n";

	my $state = {
		file => $file,
		points => \@points,
		cmdline => cmdline(),
	};
	open my $f_out, '>', $sfile;
	print $f_out Dumper( $state );
	close $f_out;
}
