// [[Rcpp::plugins(cpp11)]]
// [[Rcpp::depends(RcppThread, RcppParallel, BH)]]

#define R_NO_REMAP
#define RCPPTHREAD_OVERRIDE_THREAD 1

#include <Rcpp.h>
#include <RcppThread.h>
#include <RcppParallel.h>
#include <boost/algorithm/string.hpp>
#include "../inst/include/mecab.h"

using namespace Rcpp;

struct TextParseJoin
{
  TextParseJoin(const std::vector<std::string>* sentences, std::vector< std::vector < std::string > >& result, mecab_model_t* model)
    : sentences_(sentences), result_(result), model_(model)
  {}

  void operator()(const tbb::blocked_range<size_t>& range) const
  {
    mecab_t* tagger = mecab_model_new_tagger(model_);
    mecab_lattice_t* lattice = mecab_model_new_lattice(model_);
    const mecab_node_t* node;

    for (size_t i = range.begin(); i < range.end(); ++i) {
      std::vector< std::string > parsed;

      mecab_lattice_set_sentence(lattice, (*sentences_)[i].c_str());
      mecab_parse_lattice(tagger, lattice);

      const size_t len = mecab_lattice_get_size(lattice);
      parsed.reserve(len);

      node = mecab_lattice_get_bos_node(lattice);

      for (; node; node = node->next) {
        if (node->stat == MECAB_BOS_NODE)
          ;
        else if (node->stat == MECAB_EOS_NODE)
          ;
        else {
          std::string parsed_morph = std::string(node->surface).substr(0, node->length);
          std::vector<std::string> features;
          boost::split(features, node->feature, boost::is_any_of(","));
          parsed.push_back(parsed_morph + "/" + features[0]);
        }
      }

      result_[i] = parsed; // mutex is not needed
    }

    mecab_lattice_destroy(lattice);
    mecab_destroy(tagger);
  }

  const std::vector<std::string>* sentences_;
  std::vector< std::vector < std::string > >& result_;
  mecab_model_t* model_;
};

struct TextParseDF
{
  TextParseDF(const std::vector<std::string>* sentences, std::vector< std::vector < std::string > >& result, mecab_model_t* model)
    : sentences_(sentences), result_(result), model_(model)
  {}

  void operator()(const tbb::blocked_range<size_t>& range) const
  {
    mecab_t* tagger = mecab_model_new_tagger(model_);
    mecab_lattice_t* lattice = mecab_model_new_lattice(model_);
    const mecab_node_t* node;

    for (size_t i = range.begin(); i < range.end(); ++i) {
      std::vector< std::string > parsed;

      mecab_lattice_set_sentence(lattice, (*sentences_)[i].c_str());
      mecab_parse_lattice(tagger, lattice);

      const size_t len = mecab_lattice_get_size(lattice);
      parsed.reserve(len*4);

      node = mecab_lattice_get_bos_node(lattice);

      for (; node; node = node->next) {
        if (node->stat == MECAB_BOS_NODE)
          ;
        else if (node->stat == MECAB_EOS_NODE)
          ;
        else {
          std::string parsed_morph = std::string(node->surface).substr(0, node->length);
          std::vector<std::string> features;
          boost::split(features, node->feature, boost::is_any_of(","));
          parsed.push_back(parsed_morph);
          parsed.push_back(features[0]);
          parsed.push_back(features[1]);
          // For parsing unk-feature when using Japanese MeCab and IPA-dict.
          if (features.size() > 7) {
            parsed.push_back(features[7]);
          } else {
            parsed.push_back("*");
          }
        }
      }

      result_[i] = parsed; // mutex is not needed
    }

    mecab_lattice_destroy(lattice);
    mecab_destroy(tagger);
  }

  const std::vector<std::string>* sentences_;
  std::vector< std::vector < std::string > >& result_;
  mecab_model_t* model_;
};

struct TextParse
{
  TextParse(const std::vector<std::string>* sentences, std::vector< std::vector < std::string > >& result, mecab_model_t* model)
    : sentences_(sentences), result_(result), model_(model)
  {}

  void operator()(const tbb::blocked_range<size_t>& range) const
  {
    mecab_t* tagger = mecab_model_new_tagger(model_);
    mecab_lattice_t* lattice = mecab_model_new_lattice(model_);
    const mecab_node_t* node;

    for (size_t i = range.begin(); i < range.end(); ++i) {
      std::vector< std::string > parsed;

      mecab_lattice_set_sentence(lattice, (*sentences_)[i].c_str());
      mecab_parse_lattice(tagger, lattice);

      const size_t len = mecab_lattice_get_size(lattice);
      parsed.reserve(len*2);

      node = mecab_lattice_get_bos_node(lattice);

      for (; node; node = node->next) {
        if (node->stat == MECAB_BOS_NODE)
          ;
        else if (node->stat == MECAB_EOS_NODE)
          ;
        else {
          std::string parsed_morph = std::string(node->surface).substr(0, node->length);
          // add join mechanism
          std::vector<std::string> features;
          boost::split(features, node->feature, boost::is_any_of(","));
          parsed.push_back(parsed_morph);
          parsed.push_back(features[0]);
        }
      }

      result_[i] = parsed; // mutex is not needed
    }

    mecab_lattice_destroy(lattice);
    mecab_destroy(tagger);
  }

  const std::vector<std::string>* sentences_;
  std::vector< std::vector < std::string > >& result_;
  mecab_model_t* model_;
};

//' Call POS Tagger via `tbb::parallel_for` and return a named list.
//'
//' @param text Character vector.
//' @param sys_dic String scalar.
//' @param user_dic String scalar.
//' @return named list.
//'
//' @name posParallelJoinRcpp
//' @keywords internal
//' @export
//
// [[Rcpp::interfaces(r, cpp)]]
// [[Rcpp::export]]
List posParallelJoinRcpp(std::vector<std::string> text, std::string sys_dic, std::string user_dic) {

  std::vector< std::vector < std::string > > results(text.size());
  List result;

  std::string args = "";
  if (sys_dic != "") {
    args.append(" -d ");
    args.append(sys_dic);
  }
  if (user_dic != "") {
    args.append(" -u ");
    args.append(user_dic);
  }

  // lattice model
  mecab_model_t* model;

  // create model
  model = mecab_model_new2(args.c_str());
  if (!model) {
    Rcerr << "model is NULL" << std::endl;
    return R_NilValue;
  }

  // parallel argorithm with Intell TBB
  // RcppParallel doesn't get CharacterVector as input and output
  TextParseJoin func = TextParseJoin(&text, results, model);
  tbb::parallel_for(tbb::blocked_range<size_t>(0, text.size()), func);

  mecab_model_destroy(model);

  // explicit type conversion
  for (size_t k = 0; k < results.size(); ++k) {
    CharacterVector resultString;
    for (size_t l = 0; l < results[k].size(); ++l) {
      String morph_copy; // type conversion to Rcpp::String and set encoding
      morph_copy = results[k][l];
      morph_copy.set_encoding(CE_UTF8);
      resultString.push_back(morph_copy);
    }
    result.push_back(resultString);
  }

  StringVector result_name(text.size());

  for (size_t h = 0; h < text.size(); ++h) {
    String character_name = text[h];
    character_name.set_encoding(CE_UTF8);
    result_name[h] = character_name;
  }

  result.names() = result_name;

  return result;
}

//' Call POS Tagger via `tbb::parallel_for` and return a data.frame
//'
//' @param text Character vector.
//' @param sys_dic String scalar.
//' @param user_dic String scalar.
//' @return data.frame.
//'
//' @name posParallelDFRcpp
//' @keywords internal
//' @export
//
// [[Rcpp::interfaces(r, cpp)]]
// [[Rcpp::export]]
DataFrame posParallelDFRcpp(StringVector text, std::string sys_dic, std::string user_dic) {

  std::vector< std::vector < std::string > > results(text.size());
  std::vector< std::string > input = as<std::vector< std::string > >(text);

  IntegerVector doc_id;
  IntegerVector sentence_id;
  IntegerVector token_id;
  StringVector token;
  StringVector pos;
  StringVector subtype;
  StringVector analytic;

  String token_t;
  String pos_t;
  String subtype_t;
  String analytic_t;

  int doc_number = 0;
  int sentence_number = 1;
  int token_number = 1;
  StringVector text_names;

  std::string args = "";
  if (sys_dic != "") {
    args.append(" -d ");
    args.append(sys_dic);
  }
  if (user_dic != "") {
    args.append(" -u ");
    args.append(user_dic);
  }

  // lattice model
  mecab_model_t* model;

  // create model
  model = mecab_model_new2(args.c_str());
  if (!model) {
    Rcerr << "model is NULL" << std::endl;
    return R_NilValue;
  }

  // parallel argorithm with Intell TBB
  // RcppParallel doesn't get CharacterVector as input and output
  TextParseDF func = TextParseDF(&input, results, model);
  tbb::parallel_for(tbb::blocked_range<size_t>(0, input.size()), func);

  mecab_model_destroy(model);

  // explicit type conversion
  for (size_t k = 0; k < results.size(); ++k) {
    for (size_t l = 0; l < results[k].size(); l += 4) {
      token_t = results[k][l];
      pos_t = results[k][l + 1];
      subtype_t = results[k][l + 2];
      analytic_t = results[k][l + 3];

      // if (subtype_t == "*") {
      //   subtype_t = "";
      // }
      // if (analytic_t == "*") {
      //   analytic_t = "";
      // }

      token_t.set_encoding(CE_UTF8);
      pos_t.set_encoding(CE_UTF8);
      subtype_t.set_encoding(CE_UTF8);
      analytic_t.set_encoding(CE_UTF8);

      token.push_back(token_t);
      pos.push_back(pos_t);
      subtype.push_back(subtype_t);
      analytic.push_back(analytic_t);

      token_id.push_back(token_number);
      token_number++;

      // append sentence_id and token_id
      sentence_id.push_back(sentence_number);
      if (token_t == "." or token_t == "。") {
        sentence_number++;
        token_number = 1;
      }

      // append doc_id
      doc_id.push_back(doc_number + 1);

    }
    sentence_number = 1;
    token_number = 1;
    doc_number++;
  }

  return DataFrame::create(
    _["doc_id"] = doc_id,
    _["sentence_id"] = sentence_id,
    _["token_id"] = token_id,
    _["token"] = token,
    _["pos"] = pos,
    _["subtype"] = subtype,
    _["analytic"] = analytic,
    _["stringsAsFactors"] = false);
}

//' Call POS Tagger via `tbb::parallel_for` and return a list of named character vectors.
//'
//' @param text Character vector.
//' @param sys_dic String scalar.
//' @param user_dic String scalar.
//' @return list of named character vectors.
//'
//' @name posParallelRcpp
//' @keywords internal
//' @export
//
// [[Rcpp::interfaces(r, cpp)]]
// [[Rcpp::export]]
List posParallelRcpp( std::vector<std::string> text, std::string sys_dic, std::string user_dic ) {

  std::vector< std::vector < std::string > > results(text.size());
  List result;

  std::string args = "";
  if (sys_dic != "") {
    args.append(" -d ");
    args.append(sys_dic);
  }
  if (user_dic != "") {
    args.append(" -u ");
    args.append(user_dic);
  }

  // lattice model
  mecab_model_t* model;

  // create model
  model = mecab_model_new2(args.c_str());
  if (!model) {
    Rcerr << "model is NULL" << std::endl;
    return R_NilValue;
  }

  // parallel argorithm with Intell TBB
  // RcppParallel doesn't get CharacterVector as input and output
  TextParse func = TextParse(&text, results, model);
  tbb::parallel_for(tbb::blocked_range<size_t>(0, text.size()), func);

  mecab_model_destroy(model);

  // explicit type conversion
  for (size_t k = 0; k < results.size(); ++k) {
    CharacterVector resultString;
    CharacterVector resultTag;
    for (size_t l = 0; l < results[k].size(); l += 2) {
      String morph_copy; // type conversion to Rcpp::String and set encoding
      String tags_copy;
      morph_copy = results[k][l];
      tags_copy = results[k][l + 1];
      morph_copy.set_encoding(CE_UTF8);
      tags_copy.set_encoding(CE_UTF8);
      resultString.push_back(morph_copy);
      resultTag.push_back(tags_copy);
    }
    resultString.names() = resultTag;
    result.push_back(resultString);
  }

  StringVector result_name(text.size());

  for (size_t h = 0; h < text.size(); ++h) {
    String character_name = text[h];
    character_name.set_encoding(CE_UTF8);
    result_name[h] = character_name;
  }

  result.names() = result_name;

  return result;
}
