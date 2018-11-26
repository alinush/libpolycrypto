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

#print csv_data.to_string()  # print all data
print csv_data[['k','n','interpolation_method', 'lagr_hum', 'multiexp_hum', 'total_hum']].to_string()

# open the file in append mode, and truncate it to zero bytes if it has data
f = open(out_tex_file, "a+")

isEmpty = os.fstat(f.fileno()).st_size == 0

if not isEmpty:
    f.truncate(0)

def humanizeBytes(numBytes, precision = 2):
    result = float(numBytes)
    units = [ "bytes", "KiB", "MiB", "GiB", "TiB" ]
    i = 0;
    while result >= 1024.0 and i < len(units) - 1:
	result /= 1024.0
	i = i+1

    #string = (("%." + str(precision) + "f") % result)
    string = ("{:." + str(precision) + "f}").format(result)
    string += " "
    string += units[i]

    return string

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

# update avg_deal+verify_hum with humanized bytes, if they are 'nan':    
for idx, r in csv_data.iterrows():
    if str(r['total_hum']) == 'nan':
        #print "Humanizing row:", r.values
        csv_data.ix[idx, 'total_hum'] = humanizeMicroseconds(int(r['total_usec']))

print csv_data[['k','n','interpolation_method', 'lagr_hum', 'multiexp_hum', 'total_hum', 'total_usec']].to_string()

def write_latex_case_macro(f, data, macroName, col1, col2):
    f.write("\\newcommand{\\" + macroName + "}[1]{%\n")
    f.write("    \IfStrEqCase{#1}{")
    for _, r in data.iterrows():
        f.write("\n        {" + str(r[col1]).strip() + "}{" + str(r[col2]).strip() + "\\xspace}")

    f.write("}[\\textcolor{red}{\\textbf{NODATA}}]}\n\n")

write_latex_case_macro(f, csv_data[csv_data.interpolation_method == 'naive-lagr-wnk'], 'blsNaiveTime',  'n', 'total_hum')
write_latex_case_macro(f, csv_data[csv_data.interpolation_method == 'naive-lagr-wnk'], 'naiveLagrTime', 'n', 'lagr_hum')
write_latex_case_macro(f, csv_data[csv_data.interpolation_method == 'naive-lagr-wnk'], 'multiexpTime',  'n', 'multiexp_hum')
write_latex_case_macro(f, csv_data[csv_data.interpolation_method == 'fft-eval'],       'blsEffTime',    'n', 'total_hum')
write_latex_case_macro(f, csv_data[csv_data.interpolation_method == 'fft-eval'],       'fastLagrTime',  'n', 'lagr_hum')

# compute the improvement of FFT over naive Lagr
naive = csv_data[csv_data.interpolation_method == 'naive-lagr-wnk']['total_usec']
eff = csv_data[csv_data.interpolation_method == 'fft-eval']['total_usec']
improv = naive.values / eff.values.astype(float)
print eff.values
print naive.values

improv_data = pandas.concat(
    [
        pandas.DataFrame(csv_data.n.unique(), columns=["n"]),
        pandas.DataFrame(improv, columns=["improv"])
    ],
    axis=1)

improv_data['improv'] = improv_data['improv'].round(decimals=2)

# extract the threshold # of players n at which we beat naive
min_improv = 1.2
outperform = improv_data[(improv_data.improv > 1.2) & (improv_data.n > 64)].copy() # we might beat the naive scheme at small thresholds too, but then later on we don't beat it anymore
outperform.reset_index(drop=True, inplace=True) # because copy() does not renumber the rows of the dataframe
outperform.sort_values(by='improv', ascending=True)
outperform_num = int(outperform.ix[0]['n'])

improv_data['improv'] = improv_data['improv'].astype(str) + improvLatexSymb
#print improv_data

write_latex_case_macro(f, improv_data, 'blsTimeImprov', 'n', 'improv')

print "Starts outperforming naive at:", outperform_num
f.write("\\newcommand{\\blsOutperformN}{" + str(outperform_num) + "}\n")
