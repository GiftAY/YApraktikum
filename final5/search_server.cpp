#include "search_server.h"

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status,
    const std::vector<int>& ratings) {
    if ((document_id < 0)) {
        throw std::invalid_argument("ID can't be less than zero"s);
    }
    if (documents_.count(document_id) > 0) {
        throw std::invalid_argument("ID already exists"s);
    }
    std::vector<std::string> words;
    if (!SplitIntoWordsNoStop(document, words)) {
        throw std::invalid_argument("invalid character(s) in word"s);
    }

    const double inv_word_count = 1.0 / words.size();
    for (const std::string& word : words) {
       //word_to_document_freqs_[word][document_id] += inv_word_count;
       doc_to_word_freqs_[document_id][word] += inv_word_count;
    }

    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_id_.emplace(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {

    return FindTopDocuments(
        raw_query,
        [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {

    Query query;
    if (!ParseQuery(raw_query, query)) {
        throw std::invalid_argument("invalid request");
    }
    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {

        if (doc_to_word_freqs_.at(document_id).count(word)) {
            matched_words.push_back(word);
        }
        
    }
    for (const std::string& word : query.minus_words) {
        
        if (doc_to_word_freqs_.at(document_id).count(word)) {
            matched_words.clear();
            break;
        }
        
    }

    return { matched_words, documents_.at(document_id).status };
}

//O(log N)
const std::map<std::string, double>& SearchServer::GetWordFrequencies(const int document_id) const noexcept {
    //binary_search  
    /*
    if (doc_to_word_freqs_.count(document_id)) {
        return doc_to_word_freqs_.at(document_id);
    }    
    */
    const std::map<int, std::map<std::string, double>>::const_iterator it = 
        std::find_if(doc_to_word_freqs_.begin(), doc_to_word_freqs_.end(), [document_id](const std::pair<int, std::map<std::string, double>>& doc) 
        {
            return doc.first == document_id;
        });
    
    if (it != doc_to_word_freqs_.end())
        return it->second;

    const std::map<std::string, double>* empty = new std::map<std::string, double>;

    return *empty;
}

//O(W log N)
void SearchServer::RemoveDocument(const int document_id) {
    if (doc_to_word_freqs_.count(document_id)) {

        doc_to_word_freqs_.erase(document_id);

        documents_.erase(document_id);

        auto it = std::lower_bound(document_id_.begin(), document_id_.end(), document_id);
        document_id_.erase(it);    
    }
}

bool SearchServer::IsValidWord(const std::string& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

[[nodiscard]] bool SearchServer::SplitIntoWordsNoStop(const std::string& text, std::vector<std::string>& result) const {
    result.clear();
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            return false;
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    result.swap(words);
    return true;
}

[[nodiscard]] bool SearchServer::ParseQueryWord(std::string text, QueryWord& result) const {
    // Empty result by initializing it with default constructed QueryWord
    result = {};

    if (text.empty()) {
        return false;
    }
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }

    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        return false;
    }

    result = QueryWord{ text, is_minus, IsStopWord(text) };
    return true;
}

[[nodiscard]] bool SearchServer::ParseQuery(const std::string& text, Query& result) const {
    // Empty result by initializing it with default constructed Query
    result = {};
    for (const std::string& word : SplitIntoWords(text)) {
        QueryWord query_word;
        if (!ParseQueryWord(word, query_word)) {
            return false;
        }
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            }
            else {
                result.plus_words.insert(query_word.data);
            }
        }
    }
    return true;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    int size = 0;
    for (const auto& [doc, content] : doc_to_word_freqs_) {
        if (content.count(word)) {
            size++;
        }
    }
    return log(GetDocumentCount() * 1.0 / size);
}