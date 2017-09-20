#!/usr/bin/perl -W
#
##############################################################################
#
# e2xml2rss
#
# by Sergey Goldgaber
#
# This program converts everything2's XML to RSS
#
# $Revision: 1.1 $
# $Date: 2005/05/02 22:44:09 $
#
##############################################################################
#
# This software is released under the Hacktivismo Enhanced-Source Software
# License Agreement (see LICENSE file), with the additional provision that
# no part of this software may be used to wage or support war, or by the
# military or military contractors.
#
##############################################################################
#
# Please see the "README" file for more information.
#
##############################################################################

# NOTE: All global variables are in CAPS

################################################
# BEGIN User editable configuration {{{1
#

# List full paths to programs here, if necessary
$EGREP      = "egrep" ;
$MKTEMP     = "mktemp" ;
$WGET       = "wget" ;
$XMLSTARLET = "xmlstarlet" ;

# Temporary files go here
$TEMP_DIR = "/tmp" ;

# e2 New Writeups Ticker URL
$TICKER_URL = "http://everything2.com/index.pl?node=New%20Writeups%20XML%20Ticker" ;

# The name of this RSS channel
$CHANNEL_TITLE = "Everything2" ;

# A description of the channel
$CHANNEL_DESCRIPTION = "Everything2" ;

# A link to the channel's web page
$CHANNEL_LINK = "http://www.everything2.com" ;

# Node link prefix
$NODE_LINK = "http://everything2.com/index.pl?node_id=" ;

#
# END User editable configuration }}}
################################################

############################################
# BEGIN Generate temporary file {{{1
#
# Get the name of this program and remove the path prefix
$THIS_PROG_NAME = $0 ;
$THIS_PROG_NAME = `basename $THIS_PROG_NAME` ;
chomp $THIS_PROG_NAME ;

$MKTEMP_TEMPLATE = "$TEMP_DIR/$THIS_PROG_NAME.XXXXXX" ; # "man mktemp" for more info

# Generate a unique file to write to:
$XML_FILE = `$MKTEMP $MKTEMP_TEMPLATE` ;
chomp $XML_FILE ;

# END Generate temporary file }}}
############################################

# List nodes
sub getNodes { #{{{1
    my @Nodes = () ;
    my $Node = "" ;
    my $NodeID = "" ;
    my $COMMAND = "" ;
    my $IGNORED = "" ;

    $COMMAND = "$XMLSTARLET el -v $XML_FILE | " . # List all vals in XML_FILE
                  "$EGREP node | " . # Find just the ones labled node
                  "$EGREP -v 'author|parent'" ; # Discard authors and parents
    @Nodes = `$COMMAND` ;

    # @Nodes should now contain lines such as:
    #   newwriteups/wu/e2link[@node_id='1718486']

    # Convert each node to its node_id
    for( my $i = 0 ; $i < $#Nodes ; $i++ ) {
        $Node = $Nodes[$i] ;
        chomp $Node ;
        ( $IGNORED, $NodeID, $IGNORED ) = split( /'/, $Node ) ;
        $Nodes[$i] = $NodeID ;
    }

    return @Nodes ;
} #}}}

# Print RSS Header
sub printRSS_Header { #{{{1
    print <<RSS_Header;
<?xml version="1.0"?>
<rss version="2.0">

RSS_Header

    return ;
} #}}}

# Print RSS Footer
sub printRSS_Footer { #{{{1
    print <<RSS_Footer;

</rss>
RSS_Footer

    return ;
} #}}}

# Print Channel Header
sub printChannel_Header { #{{{1
    print <<ChannelHeader;
<channel>
<title>$CHANNEL_TITLE</title>
<description>$CHANNEL_DESCRIPTION</description>
<link>$CHANNEL_LINK</link>

ChannelHeader
    return ;
} #}}}

# Print Channel Footer
sub printChannel_Footer { #{{{1
    print <<ChannelFooter;

</channel>
ChannelFooter

    return ;
} #}}}

# Print nodes
sub printNodes { #{{{1
    my $COMMAND = "" ;
    my @Nodes = @_ ;
    my $NodeID = "" ;
    my $NodeName = "" ;

    # Fetch and print each node
    for( my $i = 0 ; $i < $#Nodes ; $i++ ) { #{{{2
        $NodeID = $Nodes[$i] ;

        # Get node title by node_id
        $COMMAND = "$XMLSTARLET sel -t -v \"//e2link[\@node_id='$NodeID']\" " .
                   $XML_FILE ;

        $NodeName = `$COMMAND` ;
        chomp $NodeName ;

        # Print node title and node_id as an RSS item
        print <<ITEM
<item>
  <title>$NodeName</title>
  <description>$NodeName</description>
  <link>$NODE_LINK$NodeID</link>
</item>

ITEM

    } #}}}

    return ;
} #}}}

sub getTicker { #{{{1
    my $COMMAND = "" ;

    $COMMAND = "$WGET --quiet --output-document=$XML_FILE $TICKER_URL" ;

    `$COMMAND` ;

    return ;
} #}}}

sub cleanup { #{{{1
    system( "rm $XML_FILE" ) ;
    return ;
} #}}}

sub main { #{{{1
    my @Nodes = () ;

    getTicker ;
    @Nodes = getNodes ;
    printRSS_Header ;
    printChannel_Header ;
    printNodes( @Nodes ) ;
    printChannel_Footer ;
    printRSS_Footer ;
    cleanup ;

    return ;
} #}}}

main ;
