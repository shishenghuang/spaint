#! /usr/bin/env bash

# Parameters are: iniPath outputPath datasetPath
# spaintgui is located in the parent folder

set -e

tag='Relocopt_Batch'
# We skip heads because there aren't enough subsequences to split the training set in train + validation
sequences='chess fire office pumpkin redkitchen stairs'
sequence_count=6
# sequences='redkitchen'
# sequence_count=1

parameters_file=$1
output_file=$2
dataset_root=$3

# Move to the script folder, to have relative paths.
cd `dirname $0`

# Concatenate the default parameters file with the current parameters file
default_parameters_file="default_parameters.ini"
ini_file="$tag.ini"

cat "$parameters_file" > $ini_file
cat "$default_parameters_file" >> $ini_file

spaint_path="../../spaintgui/spaintgui"
reloc_poses_path="../../spaintgui/reloc_poses/"
relocperf_path="../../relocperf/relocperf"

training_sequence='train'
evaluation_sequence='validation'

training_time=0
update_time=0
initial_relocalisation_time=0
icp_refinement_time=0
total_relocalisation_time=0

# Run spaintgui on every sequence.
for seq in $sequences; do
  times=$(CUDA_VISIBLE_DEVICES=0 "$spaint_path" -c "$dataset_root/calib.txt" -s "$dataset_root/$seq/$training_sequence/" -t Disk -s "$dataset_root/$seq/$evaluation_sequence/" -t ForceFail --pipelineType slam --batch --experimentTag "$tag"_"$seq" -f "$ini_file" --headless | tee /dev/stderr | perl -ne '/^(Training|Update|Initial Relocalisation|ICP Refinement|Total Relocalisation).*/ && s/.*? ([0-9]+) microseconds/\1/g && print')

  times_arr=($times)

  training_time=$(($training_time + ${times_arr[0]}))
  update_time=$(($update_time + ${times_arr[1]}))
  initial_relocalisation_time=$(($initial_relocalisation_time + ${times_arr[2]}))
  icp_refinement_time=$(($icp_refinement_time + ${times_arr[3]}))
  total_relocalisation_time=$(($total_relocalisation_time + ${times_arr[4]}))
done

# Average the timings (integer division is fine since the numbers are in microseconds).
training_time=$(($training_time / $sequence_count))
update_time=$(($update_time / $sequence_count))
initial_relocalisation_time=$(($initial_relocalisation_time / $sequence_count))
icp_refinement_time=$(($icp_refinement_time / $sequence_count))
total_relocalisation_time=$(($total_relocalisation_time / $sequence_count))

# Now evaluate the run.
$relocperf_path -d "$dataset_root" -r "$reloc_poses_path" -t "$tag" --useValidation > $output_file

# Append the times to the output file.
echo "$training_time $update_time $initial_relocalisation_time $icp_refinement_time $total_relocalisation_time" >> $output_file

# Remove ini file.
rm $ini_file

# Remove relocalisation poses.
for seq in $sequences; do
  rm -f "$reloc_poses_path/$tag"_"$seq"/*.txt
done
