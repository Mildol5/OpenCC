/*
 * Open Chinese Convert
 *
 * Copyright 2010-2013 BYVoid <byvoid@byvoid.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "../dictionary/datrie.h"
#include "../dictionary/text.h"
#include "../dict_group.h"
#include "../encoding.h"
#include "../utils.h"
#include <locale.h>
#include <unistd.h>
#include <darts.hh>

#include <iostream>
#include <vector>
#include <string>

using std::vector;
using std::string;

#ifndef VERSION
#define VERSION ""
#endif

#define DATRIE_SIZE 1000000
#define DATRIE_WORD_MAX_COUNT 500000
#define BUFFER_SIZE 1024

const char* OCDHEADER = "OPENCCDICTIONARY1";

struct Value {
  string value;
  size_t cursor;
  Value(string value_, size_t cursor_) : value(value_), cursor(cursor_) {
  }
};

struct Entry {
  string key;
  vector<size_t> valueIndexes;
};

vector<Entry> lexicon;
vector<Value> values;
Darts::DoubleArray dict;

void make() {
  vector<const char*> keys;
  vector<int> valueIndexes;
  keys.reserve(lexicon.size());
  for (size_t i = 0; i < lexicon.size(); i++) {
    keys.push_back(lexicon[i].key.c_str());
  }
  dict.build(lexicon.size(), &keys[0]);
}

int cmp(const void* a, const void* b) {
  return ucs4cmp(((const TextEntry*)a)->key, ((const TextEntry*)b)->key);
}

void init(const char* filename) {
  DictGroup* DictGroup = dict_group_new(NULL);
  if (dict_group_load(DictGroup, filename,
                            OPENCC_DICTIONARY_TYPE_TEXT) == -1) {
    dictionary_perror("Dictionary loading error");
    fprintf(stderr, _("\n"));
    exit(1);
  }
  Dict* dict_abs = dict_group_get_dict(DictGroup, 0);
  if (dict_abs == (Dict*)-1) {
    dictionary_perror("Dictionary loading error");
    fprintf(stderr, _("\n"));
    exit(1);
  }
  static TextEntry tlexicon[DATRIE_WORD_MAX_COUNT];
  /* TODO add datrie support */
  Dict* dictionary = dict_abs->dict;
  size_t lexicon_count = dict_text_get_lexicon(dictionary, tlexicon);
  qsort(tlexicon, lexicon_count, sizeof(tlexicon[0]), cmp);
  size_t lexicon_cursor = 0;
  lexicon.resize(lexicon_count);
  for (size_t i = 0; i < lexicon_count; i++) {
    char* utf8_temp = ucs4_to_utf8(tlexicon[i].key, (size_t)-1);
    lexicon[i].key = utf8_temp;
    free(utf8_temp);
    size_t value_count;
    for (value_count = 0; tlexicon[i].value[value_count] != NULL; value_count++) {}
    for (size_t j = 0; j < value_count; j++) {
      lexicon[i].valueIndexes.push_back(values.size());
      char* utf8_temp = ucs4_to_utf8(tlexicon[i].value[j], (size_t)-1);
      values.push_back(Value(utf8_temp, lexicon_cursor));
      lexicon_cursor += strlen(utf8_temp);
      free(utf8_temp);
    }
  }
}

void output(const char* file_name) {
  FILE* fp = fopen(file_name, "wb");
  if (!fp) {
    fprintf(stderr, _("Can not write file: %s\n"), file_name);
    exit(1);
  }
  /*
   Binary Structure
   [OCDHEADER]
   [number of keys]
     size_t
   [number of values]
     size_t
   [bytes of values]
     size_t
   [bytes of darts]
     size_t
   [key indexes]
     size_t(index of first value) * [number of keys]
   [value indexes]
     size_t(index of first char) * [number of values]
   [value data]
     char * [bytes of values]
   [darts]
     char * [bytes of darts]
   */
  size_t numKeys = lexicon.size();
  size_t numValues = values.size();
  size_t dartsSize = dict.total_size();
  
  fwrite(OCDHEADER, sizeof(char), strlen(OCDHEADER), fp);
  fwrite(&numKeys, sizeof(size_t), 1, fp);
  fwrite(&numValues, sizeof(size_t), 1, fp);
  fwrite(&dartsSize, sizeof(size_t), 1, fp);
  
  // key indexes
  for (size_t i = 0; i < numKeys; i++) {
    size_t index = lexicon[i].valueIndexes[0];
    fwrite(&index, sizeof(size_t), 1, fp);
  }
  
  // value indexes
  for (size_t i = 0; i < numValues; i++) {
    size_t index = values[i].cursor;
    fwrite(&index, sizeof(size_t), 1, fp);
  }
  
  // value data
  for (size_t i = 0; i < numValues; i++) {
    const char* value = values[i].value.c_str();
    fwrite(value, sizeof(char), strlen(value), fp);
  }
  
  // darts
  fwrite(dict.array(), sizeof(char), dartsSize, fp);
  
  fclose(fp);
}

#ifdef DEBUG_WRITE_TEXT
void write_text_file() {
  FILE* fp;
  int i;
  fp = fopen("datrie.txt", "w");
  fprintf(fp, "%d\n", lexicon_count);
  for (i = 0; i < lexicon_count; i++) {
    char* buff = ucs4_to_utf8(lexicon[i].value, (size_t)-1);
    fprintf(fp, "%s\n", buff);
    free(buff);
  }
  for (i = 0; i < DATRIE_SIZE; i++) {
    if (dat[i].parent != DATRIE_UNUSED) {
      fprintf(fp, "%d %d %d %d\n", i, dat[i].base, dat[i].parent, dat[i].word);
    }
  }
  fclose(fp);
}

#endif /* ifdef DEBUG_WRITE_TEXT */

void show_version() {
  printf(_("\nOpen Chinese Convert (OpenCC) Dictionary Tool\nVersion %s\n\n"),
         VERSION);
}

void show_usage() {
  show_version();
  printf(_("Usage:\n"));
  printf(_("  opencc_dict -i input_file -o output_file\n\n"));
  printf(_("    -i input_file\n"));
  printf(_("      Read data from input_file.\n"));
  printf(_("    -o output_file\n"));
  printf(_("      Write converted data to output_file.\n"));
  printf(_("\n"));
  printf(_("\n"));
}

int main(int argc, char** argv) {
  static int oc;
  static char input_file[BUFFER_SIZE], output_file[BUFFER_SIZE];
  int input_file_specified = 0, output_file_specified = 0;

#ifdef ENABLE_GETTEXT
  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE_NAME, LOCALEDIR);
#endif /* ifdef ENABLE_GETTEXT */
  while ((oc = getopt(argc, argv, "vh-:i:o:")) != -1) {
    switch (oc) {
    case 'v':
      show_version();
      return 0;
    case 'h':
    case '?':
      show_usage();
      return 0;
    case '-':
      if (strcmp(optarg, "version") == 0) {
        show_version();
      } else {
        show_usage();
      }
      return 0;
    case 'i':
      strcpy(input_file, optarg);
      input_file_specified = 1;
      break;
    case 'o':
      strcpy(output_file, optarg);
      output_file_specified = 1;
      break;
    }
  }
  if (!input_file_specified) {
    fprintf(stderr, _("Please specify input file using -i.\n"));
    show_usage();
    return 1;
  }
  if (!output_file_specified) {
    fprintf(stderr, _("Please specify output file using -o.\n"));
    show_usage();
    return 1;
  }
  init(input_file);
  make();
  output(output_file);
#ifdef DEBUG_WRITE_TEXT
  write_text_file();
#endif /* ifdef DEBUG_WRITE_TEXT */
  return 0;
}