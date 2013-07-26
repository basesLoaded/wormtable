# 
# Copyright (C) 2013 by the Wormtable team, see AUTHORS.txt.
#
# This file is part of wormtable.
# 
# wormtable is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# wormtable is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with wormtable.  If not, see <http://www.gnu.org/licenses/>.
# 

from __future__ import print_function
from __future__ import division 

import wormtable as wt
import scripts.vcf2wt as vcf2wt
import scripts.wtadmin as wtadmin

import unittest
import tempfile
import shutil
import os.path
import gzip
from xml.etree import ElementTree

EXAMPLE_VCF ="test/data/example.vcf"
SAMPLE_VCF ="test/data/sample.vcf"

# VCF fixed columns
CHROM = "CHROM"
POS = "POS"
ALT = "ALT" 
REF = "REF"
FILTER = "FILTER"

class UtilityTest(unittest.TestCase):
    """
    Superclass of all wormtable tests. Create a homedir for working in
    on startup and clear it on teardown.
    """
    def setUp(self):
        self._homedir = tempfile.mkdtemp(prefix="wtutil_") 
    
    def tearDown(self):
        shutil.rmtree(self._homedir)

    def run_command(self, options=[]):
        """
        Runs the command and stores the output and exit status.
        """
        self.get_program()(options)
      
    def assert_tables_equal(self, t1, t2):
        """
        Compare the specified tables row-by-row and verify that they are 
        equal.
        """
        for r1, r2 in zip(t1, t2):
            self.assertEqual(r1, r2)



class Vcf2wtTest(UtilityTest):
    """
    Class for testing vcf2wt.
    """

    def get_program(self):
        return vcf2wt.main


    
class VcfBuildTest(object):
    """
    Class that tests the build process for a given VCF file,
    checking the consistency of the resulting table.
    """
    def setUp(self):
        super(VcfBuildTest, self).setUp()
        vcf = self.get_vcf()
        self.run_command([vcf, self._homedir, "-qf"]) 
        self._table = wt.open_table(self._homedir)
        # get some simple information about the VCF
        self._num_rows = 0
        self._info_cols = 0
        f = open(vcf, "r")
        for l in f:
            if l.startswith("#"):
                if l.startswith("##INFO"):
                    self._info_cols += 1
            else: 
                self._num_rows += 1
  
    def tearDown(self):
        self._table.close()
        super(VcfBuildTest, self).tearDown()

    def test_length(self):
        self.assertEqual(self._num_rows, len(self._table))

    def test_fixed_columns(self):
        for c in [CHROM, POS, ALT, REF, FILTER]:
            col = self._table.get_column(c)
            self.assertEqual(col.get_name(), c)
        
    def test_info_columns(self):
        t_info_cols = 0 
        for col in self._table.columns():
            c = col.get_name()
            if c.startswith("INFO."):
                t_info_cols += 1
        self.assertEqual(t_info_cols, self._info_cols)    


class BuildExampleVCFTest(VcfBuildTest, Vcf2wtTest):
    def get_vcf(self):
        return EXAMPLE_VCF

class BuildSampleVCFTest(VcfBuildTest, Vcf2wtTest):
    def get_vcf(self):
        return SAMPLE_VCF


class TestInputMethods(Vcf2wtTest):
    """
    Test if the various input methods result in the same output file. 
    """
    def __test_gzipped_input(self, input_file):
        original = os.path.join(self._homedir, "original")
        zipped = os.path.join(self._homedir, "zipped")
        self.run_command([input_file, original, "-q"]) 
        zvcf = os.path.join(self._homedir, "vcf.gz") 
        z = gzip.open(zvcf, "wb")
        with open(input_file, "rb") as f:
            z.write(f.read())
        z.close()
        self.run_command([zvcf, zipped, "-qf"]) 
        with wt.open_table(original) as t1:
            with wt.open_table(zipped) as t2:
                self.assert_tables_equal(t1, t2)
        shutil.rmtree(self._homedir)
        os.mkdir(self._homedir)

    def test_gzip(self):
        self.__test_gzipped_input(EXAMPLE_VCF)
        self.__test_gzipped_input(SAMPLE_VCF)
    
    def test_stdin(self):
        self.assertTrue(False, "not implemented yet")


class TestSchemaGeneration(Vcf2wtTest):
    """
    Test the generation of schema files.
    """
    def __test_schema_generator(self, input_file):
        table = os.path.join(self._homedir, "table.wt")
        schema = os.path.join(self._homedir, "schema.xml")
        alt_schema = os.path.join(self._homedir, "alt_schema.xml")
        self.run_command([input_file, schema, "-g"]) 
        self.assertTrue(os.path.exists(schema))
        # verify that it is valid XML
        tree = ElementTree.parse(schema)
        root = tree.getroot()
        self.assertEqual(root.tag, "schema")
        shutil.rmtree(self._homedir)
        os.mkdir(self._homedir)

    
    def test_generator(self):
        self.__test_schema_generator(EXAMPLE_VCF)
        self.__test_schema_generator(SAMPLE_VCF)


class TestSchemaBuild(VcfBuildTest):
    """
    Test the processing of schema files.
    """

    def setUp(self):
        super(TestSchemaBuild, self).setUp()
        # TODO FINISH
        # Now, alter the schema
        r = open(schema, "r")
        w = open(alt_schema, "w") 
        for l in r:
            # remove CHROM and POS
            if CHROM not in l and POS not in l:
                w.write(l)
        r.close()
        w.close()
        self.run_command([input_file, table , "-qs" + alt_schema]) 
        with wt.open_table(table) as t:
            self.assertRaises(KeyError, t.get_column, CHROM)
            self.assertRaises(KeyError, t.get_column, POS)


