#ifndef DEFINITIONS_H__
#define DEFINITIONS_H__

// define this to perform checks and print debugging info
// #define DEBUG_ON
// define this to perform viterbi pushing of forward probabilities
//  this is much faster, but may reduce accuracy
// #define VITERBI_ON
// define this to allow for monotonic search
//  reduces speed somewhat, so only use when necessary
// #define MONOTONIC_ON
// enables compression
// #define COMPRESS_ON

#include <tr1/unordered_set>
#include <tr1/unordered_map>
#include "gng/string.h"
#include "gng/symbol-set.h"
#include "gng/symbol-map.h"
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <iostream>

#define NEG_INFINITY -1e99
#define MIN_PROB 1e-99

#define TYPE_TERM 0
#define TYPE_REG 1
#define TYPE_INV 2
#define TYPE_GEN 3
#define TYPE_BASE 4

namespace pialign {

typedef unsigned PosId;
typedef double Prob;
typedef int NodeId;
typedef int WordId;

class Edge {
public:
    Edge(int start, int end) : s(start), e(end) { }
    int s;
    int e;
};
class LabeledEdge : public Edge {
public:
    LabeledEdge(int start, int end, int label) : Edge(start,end), l(label) { }
    int l;
};
class Span {
public:
    int es;
    int ee;
    int fs;
    int fe;
    Span() { }
    Span(const Span & a) : es(a.es), ee(a.ee), fs(a.fs), fe(a.fe) { }
    Span(const Edge& e, const Edge& f) : 
        es(e.s), ee(e.e), fs(f.s), fe(f.e) { }
    Span(int eStart, int eEnd, int fStart, int fEnd) : 
        es(eStart), ee(eEnd), fs(fStart), fe(fEnd) { }
    inline int length() const {
        return ee-es+fe-fs;
    }
    inline size_t getHash() const {
        size_t hash = 5381;
        hash = ((hash << 5) + hash) + es;
        hash = ((hash << 5) + hash) + ee;
        hash = ((hash << 5) + hash) + fs;
        hash = ((hash << 5) + hash) + fe;
        return hash;
    }
};

inline std::ostream & operator<< (std::ostream & out, const Span& s) {
    out << "(" << s.es << "," << s.ee << "," << s.fs << "," << s.fe << ")";
    return out;
}

class SpanHash {
public:
    size_t operator()(const Span & x) const {
        return x.getHash();
    }
};
inline bool operator==(const Span & a, const Span & b) {
    return  a.es == b.es && a.ee == b.ee && a.fs == b.fs && a.fe == b.fe;
}

class SpanNode {
public:
    Span span; // the span
    SpanNode *left, *right; // the child nodes
    WordId phraseid; // the id of this phrase
    int type; // the type of node that this is
    bool add; // whether this node was actually added to the distribution
    Prob prob; // the generative probability of this span

    SpanNode(const Span & mySpan) : span(mySpan), left(0), right(0), phraseid(-1), type(0), add(true), prob(0) { }
    ~SpanNode() { 
        if(left) delete left;
        if(right) delete right;
    }
};

template < class T >
class PairHash {
public:
    size_t operator()(const std::pair<T,T> & x) const {
        size_t hash = 5381;
        hash = ((hash << 5) + hash) + x.first;
        hash = ((hash << 5) + hash) + x.second;
        return hash;
    }
};

class WordString {

protected:
    WordId* ptr_;
    size_t len_;

public:

    WordString() : ptr_(0), len_(0) { }
    WordString(WordId* ptr, size_t len) : ptr_(ptr), len_(len) { }

    size_t length() const { return len_; }
    WordId operator[](size_t idx) const { return ptr_[idx]; }

    void setLength(size_t len) { len_ = len; }

    WordString substr(int i, int j) const { return WordString(ptr_+i,j); }

    inline size_t getHash() const {
        size_t hash = 5381;
        for(unsigned i = 0; i < len_; i++)
            hash = ((hash << 5) + hash) + ptr_[i]; /* hash * 33 + x[i] */
        return hash;
    }

    WordId* getPointer() { return ptr_; }

};

inline bool operator<(const WordString & a, const WordString & b) {
    unsigned i;
    const unsigned al = a.length(), bl = b.length(), ml=std::min(al,bl);
    for(i = 0; i < ml; i++) {
        if(a[i] < b[i]) return true;
        else if(b[i] < a[i]) return false;
    }
    return (bl != i);
}

inline bool operator==(const WordString & a, const WordString & b) {
    unsigned i;
    const unsigned al = a.length();
    if(al!=b.length())
        return false;
    for(i = 0; i < al; i++)
        if(a[i] != b[i]) return false;
    return true;
}


typedef gng::SymbolSet< std::string, WordId > WordSymbolSet;
typedef gng::SymbolMap< std::pair<WordId, WordId>, WordId, PairHash<WordId> > PairWordMap;
typedef std::tr1::unordered_map< std::pair<WordId, WordId>, Prob, PairHash<WordId> > PairProbMap;
typedef gng::SymbolMap< Span, int, SpanHash > SpanSymbolMap;
typedef std::vector< Span > SpanSet;
typedef std::vector< SpanSet > Agendas; 
typedef std::pair<Prob, Span> ProbSpan;
typedef std::vector< ProbSpan > ProbSpanSet;


class StringWordMap : public gng::SymbolMap< WordString, WordId, gng::GenericHash<WordString> > {

public:
      
    // find all active phrases in a string
    std::vector<LabeledEdge> findEdges(const WordString & str, int maxLen) const {
        int T = str.length(), jLim;
        std::vector<LabeledEdge> ret;    
        for(int i = 0; i <= T; i++) {
            jLim = std::min(i+maxLen,T);
            for(int j = i; j <= jLim; j++) {
                StringWordMap::const_iterator it = this->find(str.substr(i,j-i));
                if(it != this->end()) {
                    ret.push_back(LabeledEdge(i,j,it->second));
                }
            }
        }
        return ret;
    }

};


class SpanProbMap : public std::tr1::unordered_map< Span, Prob, SpanHash > {
public:
    Prob getProb(const Span & mySpan) const {
        SpanProbMap::const_iterator it = find(mySpan);
        return it == end() ? NEG_INFINITY : it->second;
    }
    void insertProb(const Span & mySpan, Prob myProb) {
        insert(std::pair<Span,Prob>(mySpan,myProb));
    }
};

// sample a single set of probabilities
inline int sampleProbs(std::vector<Prob> vec) {
    if(vec.size() == 0)
        throw std::runtime_error("Zero-size vector for sampling");
    if(vec.size() == 1)
        return 0;
    Prob norm = 0;
    for(unsigned i = 0; i < vec.size(); i++)
        norm += vec[i];
    int ret = 0;
    Prob left = (norm*rand())/RAND_MAX;
    while((left -= vec[ret]) > 0) ret++;
    return ret;
}

// sample a single set of log probabilities
//  return the sampled ID and the log probability of the sample
inline int sampleLogProbs(Prob* vec, unsigned size, double anneal = 1) {
    // approximate the actual max with the front and back
    Prob myMax = NEG_INFINITY, norm = 0, left;
    for(unsigned i = 0; i < size; i++) {
        vec[i] *= anneal;
        myMax = std::max(myMax,vec[i]);
    }
    for(unsigned i = 0; i < size; i++) {
        vec[i] = exp(vec[i]-myMax);
        norm += vec[i];
    }
    int ret = 0;
    left = (norm*rand())/RAND_MAX;
    while((left -= vec[ret]) > 0) {
        // std::cerr << "  sampleLogProbs ("<<norm<<","<<myMax<<"): r="<<ret<<", b="<<left+vec[ret]<<", a="<<left<<std::endl;
        ret++;
    }
    // std::cerr << "  sampleLogProbs ("<<norm<<","<<myMax<<"): r="<<ret<<", b="<<left+vec[ret]<<", a="<<left<<std::endl;
    return ret;
}
inline int sampleLogProbs(std::vector<Prob> vec, double anneal = 1) {
    return sampleLogProbs(&vec[0],vec.size(),anneal);
}

inline Prob addLogProbs(const std::vector<Prob> & probs) {
    const unsigned size = probs.size();
    Prob myMax = std::max(probs[0],probs[size-1]), norm=0;
    for(unsigned i = 0; i < probs.size(); i++)
        norm += exp(probs[i]-myMax);
    return log(norm)+myMax;
}
inline Prob addLogProbs(Prob a, Prob b) {
    Prob myMax = std::max(a,b);
#ifdef VITERBI_ON
    return myMax;
#else
    return log(exp(a-myMax)+exp(b-myMax))+myMax;
#endif
}

class Corpus {

protected:

    std::vector<WordId> data;
    std::vector<int> sentStarts;
    std::vector<WordString> sentStrings;

public:

    size_t size() const { return sentStarts.size(); }

    void addWord(WordId id) { data.push_back(id); }
    void startSentence() { sentStarts.push_back(data.size()); }
    void endSentence() { }
    void makeSentences() { 
        sentStrings.resize(sentStarts.size());
        sentStarts.push_back(data.size());
        data.push_back(0);
        for(unsigned j = 1; j < sentStarts.size(); j++) {
            sentStrings[j-1] = WordString(&data[sentStarts[j-1]], sentStarts[j]-sentStarts[j-1]);
        }
        sentStarts.pop_back();
    }


    WordString& operator[](size_t idx) {
        return sentStrings[idx];
    }
    const WordString& operator[](size_t idx) const {
        return sentStrings[idx];
    }
    const std::vector<WordId> & getData() const { return data; }
    std::vector<WordId> & getData() { return data; }

    std::vector<int> getSentIds() const { 
        std::vector<int> ret(data.size(),0);
        int sent = 0;
        for(int i = 0; i < (int)data.size(); i++) {
            if(sent < (int)sentStarts.size()-1 && sentStarts[sent+1] == i) 
                sent++;
            ret[i] = sent;
        }
        return ret;
    }

};

}

#endif
