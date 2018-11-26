#!/usr/bin/env python2.7

import matplotlib
matplotlib.use('Agg') # otherwise script does not work when invoked over SSH

import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
from matplotlib.dates import MonthLocator, DateFormatter, DayLocator, epoch2num, num2date
import itertools
import pandas
import sys
import os
import time

improvLatexSymb="\\texttimes"

if len(sys.argv) < 3:
    print "Usage:", sys.argv[0], "<output-latex> <csv-file> [<csv_file> ...]"
    sys.exit(0)

def humanizeMicroseconds(mus, precision = 2):
    result = float(mus)
    units = [ "mus", "ms", "secs", "mins", "hrs", "days", "years" ]
    numUnits = len(units)
    i = 0

    while result >= 1000.0 and i < 2:
        result /= 1000.0
        i = i+1

    while result >= 60.0 and i >= 2 and i < 4:
        result /= 60.0
        i = i+1

    if i == 4 and result >= 24.0:
        result /= 24.0
        i = i+1

    if i == 5 and result >= 365.25:
        result /= 365.25
        i = i+1

    assert(i < numUnits)
    string = ("{:." + str(precision) + "f}").format(result)
    string += " "
    string += units[i]

    return string

def add_hum_col(csv_data, usec_col, hum_col):
    for idx, r in csv_data.iterrows():
        if r[usec_col] != 'todo' and str(r[usec_col]) != 'nan':
            csv_data.ix[idx, hum_col] = humanizeMicroseconds(int(r[usec_col]))

del sys.argv[0]

out_tex_file = sys.argv[0]
del sys.argv[0]

if not out_tex_file.endswith('.tex'):
    print "ERROR: Expected .tex file as first argument"
    sys.exit(1)

data_files = [f for f in sys.argv]

print "Reading CSV files:", data_files, "..."

csv_data = pandas.concat((pandas.read_csv(f) for f in data_files), ignore_index=True)

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
dkgs_known = [ 'jf', 'ejf', 'amt' ]
csv_data.dkg.replace('feld', 'jf', inplace=True)
csv_data.dkg.replace('kate', 'ejf', inplace=True)
dkgs_file = csv_data.dkg.unique()

csv_data['end_to_end_bc_usec'] = csv_data.avg_deal_usec + csv_data.avg_verify_best_case_usec  + csv_data.avg_reconstr_bc_usec
csv_data['end_to_end_wc_usec'] = csv_data.avg_deal_usec + csv_data.avg_verify_worst_case_usec + csv_data.avg_reconstr_wc_usec

add_hum_col(csv_data, 'end_to_end_bc_usec', 'end_to_end_bc_hum') 
add_hum_col(csv_data, 'end_to_end_wc_usec', 'end_to_end_wc_hum') 

#print csv_data.to_string()  # print all data
print csv_data[['t','n','dkg','avg_deal_hum', 'avg_verify_bc_hum', 'avg_verify_wc_hum', 'avg_reconstr_bc_hum', 'avg_reconstr_wc_hum', 'end_to_end_bc_hum', 'end_to_end_wc_hum']].to_string()

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

def write_columns(f, csv_data, usec_col, hum_col, latex_col, min_improv_pct=1.2, min_improv_n=16):
    f.write("%\n")
    f.write("% Data for column '" + usec_col + "'\n")
    f.write("% (improvements must be better than " + str(min_improv_pct) + " and occur after n > " + str(min_improv_n) + ")\n")
    f.write("%\n")
    dkgs = csv_data.dkg.unique()
    #print "Unique DKGs: ", dkgs
    for dkg in dkgs:
        write_latex_case_macro(f, csv_data[csv_data.dkg == dkg], dkg + latex_col, 'n', hum_col)

    for dkg, otherDkg in itertools.combinations(dkgs, 2):
        assert dkg != otherDkg

        # assume 'dkg' beats 'otherDkg' and compute the improvement factor
        usec1 = csv_data[csv_data.dkg == dkg][usec_col]
        usec2 = csv_data[csv_data.dkg == otherDkg][usec_col]

        # check if actually 'otherDkg' beats 'dkg'
        if usec2.sum() < usec1.sum():
            tmp = usec1
            usec1 = usec2
            usec2 = tmp

            tmp = dkg;
            dkg = otherDkg;
            otherDkg = tmp;

        improv = usec2.values / usec1.values.astype(float)

        improv_data = pandas.concat(
            [
                pandas.DataFrame(csv_data.n.unique(), columns=["n"]),
                pandas.DataFrame(improv, columns=["improv"])
            ],
            axis=1)
        improv_data['improv'] = improv_data['improv'].round(decimals=2)
        #print "Improv for", dkg, "vs", otherDkg, "DKG"
        #print improv_data

        # extract the threshold # of players n at which 'dkg' beats 'otherDkg'
        # we might beat the naive scheme at small thresholds too, but then later on we don't beat it anymore
        outperform = improv_data[(improv_data.improv > 1.2) & (improv_data.n > min_improv_n)].copy() 
        #print "Outperform for", dkg, "vs", otherDkg, "DKG"
        #print outperform
        if outperform.empty:
            print "WARNING:", dkg, "vs", otherDkg, "DKG are the same on '%s'" % usec_col
            return

        outperform.reset_index(drop=True, inplace=True) # because copy() does not renumber the rows of the dataframe
        outperform.sort_values(by='improv', ascending=True)
        outperform_num = int(outperform.ix[0]['n'])

        improv_data['improv'] = improv_data['improv'].astype(str) + improvLatexSymb
        #print improv_data

        write_latex_case_macro(f, improv_data, dkg + latex_col + 'ImprovOver' + otherDkg, 'n', 'improv')

        print dkg, "starts outperforming", otherDkg, "for", latex_col, "at:", outperform_num
        f.write("\\newcommand{\\" + dkg + latex_col + "OutperformN" + otherDkg + "}{" + str(outperform_num) + "}\n")
        f.write("\n\n")

write_columns(f, csv_data, 'avg_deal_usec', 'avg_deal_hum', 'DkgDealTime')
write_columns(f, csv_data, 'avg_verify_best_case_usec',  'avg_verify_bc_hum', 'DkgVerifyBcTime', min_improv_n=3)
write_columns(f, csv_data, 'avg_verify_worst_case_usec', 'avg_verify_wc_hum', 'DkgVerifyWcTime', min_improv_n=3)
write_columns(f, csv_data, 'avg_reconstr_bc_usec',  'avg_reconstr_bc_hum', 'DkgReconstrBcTime', min_improv_n=3)
write_columns(f, csv_data, 'avg_reconstr_wc_usec',  'avg_reconstr_wc_hum', 'DkgReconstrWcTime', min_improv_n=3)
write_columns(f, csv_data, 'end_to_end_bc_usec', 'end_to_end_bc_hum', 'DkgEndToEndBcTime')
write_columns(f, csv_data, 'end_to_end_wc_usec', 'end_to_end_wc_hum', 'DkgEndToEndWcTime')
