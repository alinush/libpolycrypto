#!/usr/bin/env python2.7

import matplotlib
matplotlib.use('Agg') # otherwise script does not work when invoked over SSH

import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
from matplotlib.dates import MonthLocator, DateFormatter, DayLocator, epoch2num, num2date
import pandas
import sys
import os
import time

improvLatexSymb="\\texttimes"

if len(sys.argv) < 3:
    print "Usage:", sys.argv[0], "<output-latex> <csv-file> [<csv_file> ...]"
    sys.exit(0)

del sys.argv[0]

out_tex_file = sys.argv[0]
del sys.argv[0]

if not out_tex_file.endswith('.tex'):
    print "ERROR: Expected .tex file as first argument"
    sys.exit(1)

data_files = [f for f in sys.argv]

print "Reading CSV files:", data_files, "..."

csv_data = pandas.concat((pandas.read_csv(f) for f in data_files))

#print "Raw:"
#print csv_data.to_string()
#print csv_data.columns
#print csv_data['dictSize'].values

#print "Averaged:"

minN = csv_data.n.unique().min();
maxN = csv_data.n.unique().max();

print "min N:", minN
print "max N:", maxN

# we specify the DKGs here in a specific order so they are plotted with the right colors
dkgs_known = [ "eJF-DKG", "AMT DKG", "JF-DKG" ]
dkgs_file = csv_data.dkg.unique()

print csv_data.to_string()  # print all data

print "DKGs in file:", dkgs_file
print "DKGs known:  ", dkgs_known

# open the file in append mode, and truncate it to zero bytes if it has data
f = open(out_tex_file, "a+")

isEmpty = os.fstat(f.fileno()).st_size == 0

if not isEmpty:
    f.truncate(0)

def write_latex_case_macro(f, data, macroName, col1, col2):
    f.write("\\newcommand{\\" + macroName + "}[1]{%\n")
    f.write("    \IfStrEqCase{#1}{")
    for _, r in data.iterrows():
        f.write("\n        {" + str(r[col1]).strip() + "}{" + str(r[col2]).strip() + "\\xspace}")

    f.write("}[\\textcolor{red}{\\textbf{NODATA}}]}\n\n")

write_latex_case_macro(f, csv_data[csv_data.dkg == 'JF-DKG'],   'jfDkgDownload', 'n', 'download_bw_hum')
write_latex_case_macro(f, csv_data[csv_data.dkg == 'AMT DKG'], 'amtDkgDownload', 'n', 'download_bw_hum')
write_latex_case_macro(f, csv_data[csv_data.dkg == 'eJF-DKG'], 'ejfDkgDownload', 'n', 'download_bw_hum')
write_latex_case_macro(f, csv_data[csv_data.dkg == 'JF-DKG'],   'jfDkgUpload', 'n', 'upload_bw_hum')
write_latex_case_macro(f, csv_data[csv_data.dkg == 'AMT DKG'], 'amtDkgUpload', 'n', 'upload_bw_hum')
write_latex_case_macro(f, csv_data[csv_data.dkg == 'eJF-DKG'], 'ejfDkgUpload', 'n', 'upload_bw_hum')
write_latex_case_macro(f, csv_data[csv_data.dkg == 'JF-DKG'],   'jfDkgComm', 'n', 'comm_bw_hum')
write_latex_case_macro(f, csv_data[csv_data.dkg == 'AMT DKG'], 'amtDkgComm', 'n', 'comm_bw_hum')
write_latex_case_macro(f, csv_data[csv_data.dkg == 'eJF-DKG'], 'ejfDkgComm', 'n', 'comm_bw_hum')

# compute the download overhead of AMT over eJF-DKG
ejf_bytes = csv_data[csv_data.dkg == 'eJF-DKG']['download_bw_bytes']
#print ejf_bytes
#print
amt_bytes = csv_data[csv_data.dkg == 'AMT DKG']['download_bw_bytes']
#print amt_bytes
#print
overhead = amt_bytes.values.astype(float) / ejf_bytes.values

overhead_data = pandas.concat(
    [
        pandas.DataFrame(csv_data.n.unique(), columns=["n"]),
        pandas.DataFrame(overhead, columns=["overhead"])
    ],
    axis=1)
overhead_data['overhead'] = overhead_data['overhead'].round(decimals=2)
overhead_data['overhead'] = overhead_data['overhead'].astype(str) + improvLatexSymb
#print overhead_data

write_latex_case_macro(f, overhead_data, 'amtDkgDownloadOverhead', 'n', 'overhead')

# compute the upload overhead of AMT over eJF-DKG
ejf_bytes = csv_data[csv_data.dkg == 'eJF-DKG']['upload_bw_bytes']
#print ejf_bytes
#print
amt_bytes = csv_data[csv_data.dkg == 'AMT DKG']['upload_bw_bytes']
#print amt_bytes
#print
overhead = amt_bytes.values.astype(float) / ejf_bytes.values

overhead_data = pandas.concat(
    [
        pandas.DataFrame(csv_data.n.unique(), columns=["n"]),
        pandas.DataFrame(overhead, columns=["overhead"])
    ],
    axis=1)
overhead_data['overhead'] = overhead_data['overhead'].round(decimals=2)
overhead_data['overhead'] = overhead_data['overhead'].astype(str) + improvLatexSymb
#print overhead_data

write_latex_case_macro(f, overhead_data, 'amtDkgUploadOverhead', 'n', 'overhead')

# compute the download+upload overhead of AMT over eJF-DKG
ejf_bytes = csv_data[csv_data.dkg == 'eJF-DKG']['comm_bw_bytes']
#print ejf_bytes
#print
amt_bytes = csv_data[csv_data.dkg == 'AMT DKG']['comm_bw_bytes']
#print amt_bytes
#print
overhead = amt_bytes.values.astype(float) / ejf_bytes.values

overhead_data = pandas.concat(
    [
        pandas.DataFrame(csv_data.n.unique(), columns=["n"]),
        pandas.DataFrame(overhead, columns=["overhead"])
    ],
    axis=1)
overhead_data['overhead'] = overhead_data['overhead'].round(decimals=2)
overhead_data['overhead'] = overhead_data['overhead'].astype(str) + improvLatexSymb
#print overhead_data

write_latex_case_macro(f, overhead_data, 'amtDkgCommOverhead', 'n', 'overhead')
