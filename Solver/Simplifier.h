/**************************************************************************************************

Solver.C -- (C) Niklas Een, Niklas Sorensson, 2004

A simple Chaff-like SAT-solver with support for incremental SAT.

**************************************************************************************************/

#ifndef SIMPLIFIER_H
#define SIMPLIFIER_H

#include "Solver.h"
#include "Queue.h"
#include "TmpFiles.h"

// For derivation output (verbosity level 2)
#define L_IND    "%-*d"
#define L_ind    decisionLevel()*3+3,decisionLevel()
#define L_LIT    "%sx%d"
#define L_lit(p) p.sign()?"~":"", p.var()

inline string name(const lbool& p) {
    if (p.isUndef())
        return "l_Undef";
    else {
        if (p.getBool())
            return "l_True";
        else
            return "l_False";
    }
}

template <class T>
uint32_t calcAbstraction(const T& ps) {
    uint32_t abstraction = 0;
    for (uint32_t i = 0; i != ps.size(); i++)
        abstraction |= 1 << (ps[i].toInt() & 31);
    return abstraction;
}

#pragma pack(push)
#pragma pack(1)
class ClauseSimp
{
    public:
        ClauseSimp(Clause* c, const uint32_t _index) :
            clause(c)
            , index(_index)
        {
            abst = calcAbstraction(*c);
        }
    
        Clause* clause;
        uint32_t abst;
        uint32_t index;
};
#pragma pack(pop)

class CSet {
    vec<uint>       where;  // Map clause ID to position in 'which'.
    vec<ClauseSimp> which;  // List of clauses (for fast iteration). May contain 'Clause_NULL'.
    vec<uint>       free;   // List of positions holding 'Clause_NULL'.
    
    public:
        //ClauseSimp& operator [] (uint32_t index) { return which[index]; }
        int size(void) const { return which.size(); }
        int nElems(void) const { return which.size() - free.size(); }
        
        bool add(ClauseSimp& c) {
            assert(c.clause != NULL);
            where.growTo(c.index+1, -1);
            if (where[c.index] != -1) {
                //already in, only update
                which[where[c.index]].abst = c.abst;
                return true;
            }
            if (free.size() > 0){
                where[c.index] = free.last();
                which[free.last()] = c;
                free.pop();
            }else{
                where[c.index] = which.size();
                which.push(c);
            }
            return false;
        }
        
        bool exclude(ClauseSimp& c) {
            assert(c.clause != NULL);
            if (c.index >= where.size() || where[c.index] == -1) {
                //not inside
                return false;
            }
            free.push(where[c.index]);
            which[where[c.index]].clause = NULL;
            where[c.index] = -1;
            return true;
        }
        
        void clear(void) {
            for (int i = 0; i < which.size(); i++)  {
                if (which[i].clause != NULL) {
                    where[which[i].index] = -1;
                }
            }
            which.clear();
            free.clear();
        }
        
        void update(ClauseSimp& c) {
            if (c.index >= where.size() || where[c.index] == -1)
                return;
            which[where[c.index]].abst = c.abst;
        }
        
        class iterator
        {
            public:
                iterator(ClauseSimp* _it) :
                    it(_it)
                {}
                
                void operator++()
                {
                    it++;
                }
                
                const bool operator!=(const iterator& iter) const
                {
                    return (it != iter.it);;
                }
                
                ClauseSimp& operator*() {
                    return *it;
                }
                
                ClauseSimp*& operator->() {
                    return it;
                }
            private:
                ClauseSimp* it;
        };
        
        iterator begin()
        {
            return iterator(which.getData());
        }
        
        iterator end()
        {
            return iterator(which.getData() + which.size());
        }
};

enum OccurMode { occ_Off, occ_Permanent, occ_All };

class Simplifier
{
public:
    
    Simplifier(Solver& S2);
    
    //Main
    vec<ClauseSimp>     clauses;
    vec<char>           touched;        // Is set to true when a variable is part of a removed clause. Also true initially (upon variable creation).
    vec<Var>            touched_list;   // A list of the true elements in 'touched'.
    CSet                cl_touched;     // Clauses strengthened.
    CSet                cl_added;       // Clauses created.
    vec<char>           var_elimed;     // 'eliminated[var]' is TRUE if variable has been eliminated.
        
    //Other
    vec<vec<ClauseSimp> >  occur;       // 'occur[index(lit)]' is a list of constraints containing 'lit'.
    OccurMode              occur_mode;  // What clauses to keep in the occur lists.
    
    //IO
    FILE*               elim_out;       // File storing eliminated clauses (needed to calculate model).
    char*               elim_out_file;  // (name of file)
    vec<vec<ClauseSimp>* > iter_vecs;   // Vectors currently used for iterations. Removed clauses will be looked up and replaced by 'Clause_NULL'.
    vec<CSet* > iter_sets;   // Sets currently used for iterations.
    
    // Temporaries (to reduce allocation overhead):
    //
    vec<bool>           seen_tmp;       // (used in various places)
    vec<Lit>            io_tmp;         // (used for reading/writing clauses from/to disk)
    
    // The Solver
    //
    Solver& solver;
    void addFromSolver(vec<Clause*>& cs);
    
    // Database management:
    //
    void createTmpFiles(const char* filename) {
        if (filename == NULL)
            elim_out = createTmpFile("/tmp/tmp_elims__", "w+b", elim_out_file);
        else
            elim_out = fopen(filename, "w+b"),
            elim_out_file = NULL;
    }
    void deleteTmpFiles(void) { if (elim_out_file != NULL) deleteTmpFile(elim_out_file, true); }
    void registerIteration  (CSet& iter_set) { iter_sets.push(&iter_set); }
    void unregisterIteration(CSet& iter_set) { remove(iter_sets, &iter_set); }
    void registerIteration  (vec<ClauseSimp>& iter_vec) { iter_vecs.push(&iter_vec); }
    void unregisterIteration(vec<ClauseSimp>& iter_vec) { remove(iter_vecs, &iter_vec); }
    
    // Subsumption:
    //
    void unlinkClause(ClauseSimp cc, Var elim = var_Undef);
    void unlinkModifiedClause(vec<Lit>& cl, ClauseSimp c);
    void touch(const Var x);
    void touch(const Lit p);
    bool updateOccur(Clause& c);
    void findSubsumed(ClauseSimp& ps, vec<ClauseSimp>& out_subsumed);
    void findSubsumed(vec<Lit>& ps, vec<ClauseSimp>& out_subsumed);
    bool isSubsumed(Clause& ps);
    void subsume0(ClauseSimp& ps, int& counter = *(int*)NULL);
    void subsume1(ClauseSimp& ps, int& counter = *(int*)NULL);
    const bool simplifyBySubsumption(bool with_var_elim = true);
    void smaller_database(int& clauses_subsumed, int& literals_removed);
    void almost_all_database(int& clauses_subsumed, int& literals_removed);
    void orderVarsForElim(vec<Var>& order);
    int  substitute(Lit x, Clause& def, vec<Clause*>& poss, vec<Clause*>& negs, vec<Clause*>& new_clauses);
    Lit  findUnitDef(Var x, vec<Clause*>& poss, vec<Clause>& negs);
    bool findDef(Lit x, vec<Clause*>& poss, vec<Clause>& negs, Clause& out_def);
    bool maybeEliminate(Var x);
    void asymmetricBranching(Lit p);
    void exclude(vec<ClauseSimp>& cs, Clause* c);
    bool subsetAbst(uint64_t A, uint64_t B);
    template<class T1, class T2>
    bool subset(const T1& A, const T2& B, vec<bool>& seen);
    void updateClause(Clause& cl, ClauseSimp& c);
};

template <class T, class T2>
void maybeRemove(vec<T>& ws, const T2& elem)
{
    if (ws.size() > 0)
        removeW(ws, elem);
}

inline void Simplifier::touch(const Var x)
{
    if (!touched[x]) {
        touched[x] = 1;
        touched_list.push(x);
    }
}
inline void Simplifier::touch(const Lit p)
{
    touch(p.var());
}

inline bool Simplifier::updateOccur(Clause& c)
{
    return occur_mode == occ_All || (occur_mode == occ_Permanent && !c.learnt());
}


inline bool Simplifier::subsetAbst(uint64_t A, uint64_t B)
{
    return !(A & ~B);
}

// Assumes 'seen' is cleared (will leave it cleared)
template<class T1, class T2>
bool Simplifier::subset(const T1& A, const T2& B, vec<bool>& seen)
{
    for (uint i = 0; i != B.size(); i++)
        seen[B[i].toInt()] = 1;
    for (uint i = 0; i != A.size(); i++) {
        if (!seen[A[i].toInt()]) {
            for (uint i = 0; i != B.size(); i++)
                seen[B[i].toInt()] = 0;
            return false;
        }
    }
    for (uint i = 0; i != B.size(); i++)
        seen[B[i].toInt()] = 0;
    return true;
}



inline void dump(Clause& c, bool newline = true, FILE* out = stdout)
{
    fprintf(out, "{");
    for (int i = 0; i < c.size(); i++)
        fprintf(out, " "L_LIT, L_lit(c[i]));
    
    fprintf(out, " }%s", newline ? "\n" : "");
    fflush(out);
}

inline void dump(Solver& S, Clause& c, bool newline = true, FILE* out = stdout)
{
    fprintf(out, "{");
    for (int i = 0; i < c.size(); i++)
        fprintf(out, " "L_LIT":%c", L_lit(c[i]), name(S.value(c[i])).c_str());
    
    fprintf(out, " }%s", newline ? "\n" : "");
    fflush(out);
}

inline void dump(const vec<Lit>& c, bool newline = true, FILE* out = stdout)
{
    fprintf(out, "{");
    for (int i = 0; i < c.size(); i++)
        fprintf(out, " "L_LIT, L_lit(c[i]));
    
    fprintf(out, " }%s", newline ? "\n" : "");
    fflush(out);
}

inline void dump(Solver& S, vec<Lit>& c, bool newline = true, FILE* out = stdout)
{
    fprintf(out, "{");
    for (int i = 0; i < c.size(); i++)
        fprintf(out, " "L_LIT":%c", L_lit(c[i]), name(S.value(c[i])).c_str());
    
    fprintf(out, " }%s", newline ? "\n" : "");
    fflush(out);
}

#endif //SIMPLIFIER_H
