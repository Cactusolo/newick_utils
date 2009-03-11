#!/bin/bash 

# Thomas Junier, 2009

# bootscan.sh - an example of bootscanning using the Newick Utilities in
# cooperation with other command-line bioinformatics tools. Requires Muscle
# [1], EMBOSS [2], and PhyML [3], GNUPlot [5]; as well as the GNU core
# utilities [4] (which you probably already have if you're running Linux). The
# PATH should be set so that these programs are found, as the script cannot use
# absolute pathnames (for portability).

# Will create files (alignment, trees, etc) in the directory in which the input
# file is found.

# References
# [1] http://www.drive5.com/muscle
# [2] http://emboss.sourceforge.net 
# [3] http://atgc.lirmm.fr/phyml
# [4] http://www.gnu.org/software/coreutils
# [5] http://www.gnuplot.info

shopt -s -o nounset

declare -r INPUT_FILE=$1
declare -r OUTGROUP=$2
declare -r REFERENCE=$3
declare -ri SLICE_WIDTH=300	# residues
declare -ri SLICE_STEP=50	# slice every SLICE_STEP residues
declare -ri BOOTSTRAPS=10	

declare -r MUSCLE_OUT=$INPUT_FILE.mfa
declare -r DISTANCES=$INPUT_FILE.dist
declare -r GNUPLOT=$INPUT_FILE.gplot


# Align sequences


echo "Aligning"
muscle -quiet -in $INPUT_FILE -out $MUSCLE_OUT


# Slice alignment


echo "Slicing alignment"
declare -r ALN_LENGTH=$(infoalign -only -alignlength $MUSCLE_OUT | head -1)
for slice_start in $(seq 1 $SLICE_STEP $((ALN_LENGTH - SLICE_WIDTH))) ; do
	slice_stop=$((slice_start+SLICE_WIDTH-1))
	seqret -osformat phylip $MUSCLE_OUT[$slice_start:$slice_stop] \
		> ${MUSCLE_OUT}_slice_${slice_start}-${slice_stop}.phy
done


# Make trees

# NOTE: the PhyML parameters below are geared towards fast (but lower quality)
# trees, to get short runtimes for purposes of demonstration. For more
# realistic trees, the runtimes can be much longer, and then you would probably
# run each job in parallel on a computing cluster, then wait for all jobs to
# finish before proceeding.

echo "Computing trees"
for slice in ${MUSCLE_OUT}_slice_*.phy; do
	echo " $slice"
	phyml $slice 0 i 1 $BOOTSTRAPS JC69 4.0 0.0 1 1.0 BIONJ y n > /dev/null
done


# Reroot trees

echo "Rerooting trees on $OUTGROUP"
for unrooted_tree in  ${MUSCLE_OUT}_slice_*.phy_phyml_tree.txt; do
	nw_reroot $unrooted_tree $OUTGROUP > ${unrooted_tree/.txt/.rr.nw}
done


# Computing distances

# This creates an array of all labels (minus the outgroup). First we extract
# the ingroup (nw_clade -s), then we list all leaf labels (nw_labels -I). We
# use the first slice, but we could use any of them.

labels=($(nw_clade -s ${MUSCLE_OUT}_slice_1-*.rr.nw $OUTGROUP | nw_labels -I -))

# This prints the distance from the reference to all labels in the list, for
# each position. At the end of the loop, the header column is removed, and the
# lines are sorted by position.

shopt -s -o xtrace
echo "Computing distances for ${labels[*]}"
for rooted_tree in ${MUSCLE_OUT}_slice_*.rr.nw ; do 
	position=${rooted_tree/*_slice_/}
	position=${position/-*}
	echo -n "$position	"	# TAB!
	# We compute a matrix of all-vs-all distances, but only keep the line for
	# the reference
	nw_distance -mm -n $rooted_tree ${labels[*]} | grep $REFERENCE
done | cut -f1,3-12 | sort -k1n > $DISTANCES

# Generate plot with GNUplot

echo "Plotting"
printf "plot '%s' using 1:2 title '%s'" $DISTANCES ${labels[0]} > $GNUPLOT
for i in $(seq 1 ${#labels[*]}); do
	printf ", '' using 1:%d title '%s'" $((i+1)) ${labels[$((i-1))]}
done > $GNUPLOT

