#' part-of-speech tagger
#'
#' \code{pos} returns part-of-speech (POS) tagged morphemes of the sentence.
#'
#' This is a basic function for MeCab part-of-speech tagger. The function gets a
#' character vector of any length and runs a loop inside C++ to provide faster
#' processing.
#'
#' You can add a user dictionary to `user_dic`. It should be compiled by
#' `mecab-dict-index`. You can find an explanation about compiling a user
#' dictionary in the \url{https://github.com/junhewk/RcppMeCab}.
#'
#' You can also set a system dictionary especially if you are using multiple
#' dictionaries (for example, using both IPA and Juman dictionary at the same time in Japanese)
#' in `sys_dic`. Using \code{options(mecabSysDic="#the path to your system dictionary")}, you can set your
#' preferred system dictionary to the R terminal.
#'
#' If you want to get a morpheme only, use `join = FALSE` to put tag names on the attribute.
#' Basically, the function will return a list of character vectors with (morpheme)/(tag) elements.
#'
#' @param sentence A character vector of any length. For analyzing multiple sentences, put them in one character vector.
#' @param join A logical to decide the output format. The default value is TRUE. If FALSE, the function will return morphemes only, and tags put in the attribute. if `format="data.frame"`, then this will be ignored.
#' @param format A data type for the result. The default value is "list". You can set this to "data.frame" to get a result as data frame format.
#' @param sys_dic A location of system MeCab dictionary. The default value is "".
#' @param user_dic A location of user-specific MeCab dictionary. The default value is "".
#' @return A string vector of POS tagged morpheme will be returned in conjoined character
#'  vector form. Element names of the list are original phrases
#'
#' @examples
#' \dontrun{
#' sentence <- c("some UTF-8 texts")
#' pos(sentence)
#' pos(sentence, join = FALSE)
#' pos(sentence, format = "data.frame")
#' pos(sentence, user_dic = "~/user_dic.dic")
#' # System dictionary example: in case of using mecab-ipadic-NEologd
#' pos(sentence, sys_dic = "/usr/local/lib/mecab/dic/mecab-ipadic-neologd/")
#' }
#'
#' @export
pos <- function(sentence, join = TRUE, format = c("list", "data.frame"), sys_dic = "", user_dic = "") {
  if (typeof(sentence) != "character") {
    if (typeof(sentence) == "factor") {
      stop("The type of input sentence is a factor. Please typesetting it with as.character().")
    } else {
      stop("The function gets a character vector only.")
    }
  }

  if (!isBlank(getOption("mecabSysDic"))) sys_dic <- getOption("mecabSysDic")

  sentence <- purrr::set_names(stringi::stri_enc_toutf8(sentence), names(sentence))
  format <- match.arg(format)
  sys_dic <- paste0(sys_dic, collapse = "")
  user_dic <- paste0(user_dic, collapse = "")

  if (format == "data.frame") {
    result <- posLoopDFRcpp(sentence, sys_dic, user_dic)
    result <- dplyr::mutate(result, dplyr::across(where(is.character), ~ dplyr::na_if(., "*")))
    if (!is.null(names(sentence))) {
      result$doc_id <- factor(
        result$doc_id,
        levels = seq_along(sentence),
        labels = names(sentence)
      )
    } else {
      result$doc_id <- factor(
        result$doc_id,
        levels = seq_along(sentence)
      )
    }
  } else {
    if (join == TRUE) {
      result <- posApplyJoinRcpp(sentence, sys_dic, user_dic)
    } else {
      result <- posApplyRcpp(sentence, sys_dic, user_dic)
    }
  }

  return(result)
}
