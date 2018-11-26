#!/usr/bin/env python2.7

import math
import os
import pandas
import sys
import time
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
from matplotlib.dates import MonthLocator, DateFormatter, DayLocator, epoch2num, num2date

def print_usage(cmd_name):
    print "Usage:", cmd_name, "<csv-file> [<col_usec>] [<col_hum>]"

cmd_path = sys.argv[0]
cmd_name = os.path.basename(cmd_path)

if len(sys.argv) < 2:
    print_usage(cmd_name)
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

del sys.argv[0]

data_files = [ sys.argv[0] ]
del sys.argv[0]

usec_col=None
hum_col=None

if len(sys.argv) >= 1:
    usec_col = sys.argv[0]
    del sys.argv[0]

    if len(sys.argv) >= 1:
        hum_col = sys.argv[0]
        del sys.argv[0]
    else:
        print "ERROR: Must specify <col_hum> after <col_usec>"
        print
        print_usage(cmd_name)
        sys.exit(1)

csv_data = pandas.concat((pandas.read_csv(f) for f in data_files))

cols = list(csv_data.columns.values)

print csv_data.to_string();

if usec_col is not None:
    for idx, r in csv_data.iterrows():
        if r[usec_col] != 'todo' and str(r[usec_col]) != 'nan':
            csv_data.ix[idx, hum_col] = humanizeMicroseconds(int(r[usec_col]))

else:
    for c_usec in cols:
        if c_usec.endswith("_usec"):
            c_hum = c_usec[:-5] + "_hum"
            print c_usec
            print c_hum

            for idx, r in csv_data.iterrows():
                if r[c_usec] != 'todo' and str(r[c_usec]) != 'nan':
                    csv_data.ix[idx, c_hum] = humanizeMicroseconds(int(r[c_usec]))

print csv_data.to_string();

csv_data.to_csv(data_files[0], float_format='%f', index=False);
