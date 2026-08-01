// apps/numkit/ide_serializer.hpp
//
// Var-data serialisation helpers for the native --ide-session protocol.
//
// Extracted from wasm/src/repl_bindings.cpp so the same JSON formats are
// shared between the WASM build (EMSCRIPTEN_BINDINGS) and the native CLI
// (runIdeSession in main.cpp).  All functions are inline so no extra TU.
//
// Public API (namespace numkit::ide):
//   getVarShapeJSON    — lightweight dimension-only query
//   getVarDataJSON     — full data for one 2-D slice (= getVarPage)
//   getVarTileJSON     — viewport tile for huge matrices (tile mode)
//   getVarStatsJSON    — aggregate stats (min/max/mean/…) for heatmaps
//   getInspectPathJSON — struct / cell drill-in path inspector
//
// Each function takes const Engine& and returns a JSON string.
// On error: {"error":"..."}.
// These formats MUST match WASM binding output (JS components use both).

#pragma once

#include <numkit/core/engine.hpp>
#include <numkit/core/value_stats.hpp>
#include <numkit/core/value_json.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace numkit { namespace ide {

namespace detail {

inline std::string escapeJSON(const std::string& s)
{
    std::string r; r.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '"':  r += "\\\""; break; case '\\': r += "\\\\"; break;
        case '\n': r += "\\n";  break; case '\r': r += "\\r";  break;
        case '\t': r += "\\t";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8]; snprintf(buf,sizeof(buf),"\\u%04x",(unsigned char)c);
                r += buf;
            } else { r += c; }
        }
    }
    return r;
}

inline std::string jnum(double x)
{ std::ostringstream o; o.precision(17); o << x; return o.str(); }

inline std::string valuePreview(const numkit::Value& val)
{
    using numkit::ValueType;
    try {
        if (val.isScalar()) {
            if (val.type()==ValueType::DOUBLE) {
                double v=val.toScalar();
                if (std::isnan(v)) return "NaN";
                if (std::isinf(v)) return v>0?"Inf":"-Inf";
                if (v==static_cast<int64_t>(v)&&std::abs(v)<1e15)
                    return std::to_string(static_cast<int64_t>(v));
                std::ostringstream os; os<<v; return os.str();
            }
            if (val.type()==ValueType::LOGICAL) return val.toBool()?"true":"false";
            if (val.type()==ValueType::COMPLEX) {
                auto c=val.toComplex(); std::ostringstream os;
                os<<c.real(); if(c.imag()>=0)os<<"+"; os<<c.imag()<<"i"; return os.str();
            }
            if (numkit::isIntegerType(val.type())) return numkit::numericCellJSON(val,0);
            if (val.type()==ValueType::SINGLE) {
                double v=val.elemAsDouble(0);
                if (std::isnan(v)) return "NaN";
                if (std::isinf(v)) return v>0?"Inf":"-Inf";
                if (v==static_cast<int64_t>(v)&&std::abs(v)<1e15)
                    return std::to_string(static_cast<int64_t>(v));
                std::ostringstream os; os<<v; return os.str();
            }
        }
        if (val.type()==ValueType::CHAR) return "'"+val.toString()+"'";
        const auto& d=val.dims();
        std::ostringstream os;
        os<<"["<<d.rows()<<"x"<<d.cols();
        if(d.is3D())os<<"x"<<d.pages();
        os<<" "<<numkit::mtypeName(val.type())<<"]";
        const ValueType pt=val.type();
        if ((numkit::isFloatType(pt)||numkit::isIntegerType(pt))&&val.numel()<=10) {
            os<<" [";
            for(size_t i=0;i<val.numel();++i){
                if(i)os<<" ";
                if(numkit::isIntegerType(pt)){os<<numkit::numericCellJSON(val,i);continue;}
                double v=val.elemAsDouble(i);
                if(v==static_cast<int64_t>(v)&&std::abs(v)<1e15)os<<static_cast<int64_t>(v);
                else os<<v;
            }
            os<<"]";
        }
        return os.str();
    } catch(...){return "<error>";}
}

inline void emitMatrixDataArray(std::ostringstream& os,
                                const numkit::Value& val, size_t page=0)
{
    using numkit::ValueType;
    const auto& d=val.dims();
    const size_t rows=d.rows(), cols=d.cols(), pageOff=page*rows*cols;
    os<<"[";
    if (val.type()==ValueType::CHAR) {
        std::string str=val.toString(); os<<"[";
        for(size_t i=0;i<str.size();++i){if(i)os<<",";os<<"\""<<escapeJSON(std::string(1,str[i]))<<"\"";}
        os<<"]";
    } else if (numkit::isRealNumericCell(val.type())) {
        for(size_t r=0;r<rows;++r){if(r)os<<",";os<<"[";
            for(size_t c=0;c<cols;++c){if(c)os<<",";os<<numkit::numericCellJSON(val,pageOff+c*rows+r);}
            os<<"]";}
    } else if (val.type()==ValueType::COMPLEX) {
        const numkit::Complex* p=val.complexData();
        for(size_t r=0;r<rows;++r){if(r)os<<",";os<<"[";
            for(size_t c=0;c<cols;++c){if(c)os<<",";
                const auto& z=p[pageOff+c*rows+r];
                std::ostringstream s;s.precision(12);s<<z.real();if(z.imag()>=0)s<<"+";s<<z.imag()<<"i";
                os<<"\""<<s.str()<<"\"";}
            os<<"]";}
    } else {
        os<<"[\""<<escapeJSON(valuePreview(val))<<"\"]";
    }
    os<<"]";
}

struct PathStep { char kind; std::string name; size_t idx; };

inline std::vector<PathStep> parseInspectPath(const std::string& s)
{
    std::vector<PathStep> steps; size_t i=0;
    while(i<s.size()){
        size_t semi=s.find(';',i);
        std::string tok=s.substr(i,semi==std::string::npos?std::string::npos:semi-i);
        i=(semi==std::string::npos)?s.size():semi+1;
        if(tok.empty())continue;
        size_t colon=tok.find(':');
        if(colon==std::string::npos)continue;
        PathStep st{};st.kind=tok[0];
        const std::string val=tok.substr(colon+1);
        if(st.kind=='f')st.name=val;
        else st.idx=static_cast<size_t>(std::strtoull(val.c_str(),nullptr,10));
        steps.push_back(st);
    }
    return steps;
}

inline const numkit::Value* resolveInspectPath(const numkit::Value& root,
                                               const std::vector<PathStep>& steps,
                                               std::vector<numkit::Value>& owned)
{
    owned.reserve(steps.size());
    const numkit::Value* cur=&root;
    for(const auto& st:steps){
        if(st.kind=='f'){if(!cur->isStruct()||!cur->hasField(st.name))return nullptr;cur=&cur->field(st.name);}
        else if(st.kind=='e'){if(st.idx>=cur->numel())return nullptr;owned.push_back(cur->elemAt(st.idx));cur=&owned.back();}
        else if(st.kind=='c'){if(!cur->isCell()||st.idx>=cur->numel())return nullptr;cur=&cur->cellAt(st.idx);}
        else return nullptr;
    }
    return cur;
}

inline std::string statsJSONFrag(const numkit::Value& val)
{
    numkit::ValueStats st;
    if(!numkit::computeValueStats(val,st))return "";
    std::ostringstream os;
    os<<"\"stats\":{\"min\":"<<jnum(st.min)<<",\"max\":"<<jnum(st.max)
      <<",\"mean\":"<<jnum(st.mean)<<",\"median\":"<<jnum(st.median)
      <<",\"mode\":"<<jnum(st.mode)<<",\"var\":"<<jnum(st.var)
      <<",\"std\":"<<jnum(st.std)<<"}";
    return os.str();
}

inline void emitInspectCell(std::ostringstream& os, const std::string& label,
                             const numkit::Value& val)
{
    const auto& d=val.dims();
    os<<"{";
    if(!label.empty())os<<"\"label\":\""<<escapeJSON(label)<<"\",";
    os<<"\"type\":\""<<numkit::mtypeName(val.type())<<"\""
      <<",\"size\":\""<<d.rows()<<"x"<<d.cols();
    if(d.is3D())os<<"x"<<d.pages();
    os<<"\",\"summary\":\""<<escapeJSON(valuePreview(val))<<"\""
      <<",\"bytes\":"<<val.deepBytes()<<",\"drill\":true";
    std::string sj=statsJSONFrag(val);
    if(!sj.empty())os<<","<<sj;
    os<<"}";
}

static constexpr size_t MATRIX_INSPECT_CAP=250000;

inline std::string emitInspectPayload(const numkit::Value& val)
{
    using numkit::ValueType;
    std::ostringstream os;
    const auto& d=val.dims();
    if(val.isStruct()){
        const auto order=val.fieldNamesInOrder();
        const size_t n=val.numel();
        os<<"{\"kind\":\"struct\",\"rows\":"<<d.rows()<<",\"cols\":"<<d.cols()
          <<",\"numel\":"<<n<<",\"fields\":[";
        for(size_t f=0;f<order.size();++f){if(f)os<<",";os<<"\""<<escapeJSON(order[f])<<"\"";}
        os<<"],\"elems\":[";
        const bool isArr=val.isStructArray();
        for(size_t e=0;e<n;++e){
            if(e)os<<",";
            const auto& fields=isArr?val.structArrayElem(e):val.structFields();
            os<<"[";
            for(size_t f=0;f<order.size();++f){
                if(f)os<<",";
                auto it=fields.find(order[f]);
                if(it==fields.end())os<<"{\"type\":\"\",\"size\":\"\",\"summary\":\"\",\"drill\":false}";
                else emitInspectCell(os,"",it->second);
            }
            os<<"]";
        }
        os<<"]}"; return os.str();
    }
    if(val.isCell()){
        const size_t rows=d.rows(),cols=d.cols(),n=val.numel();
        os<<"{\"kind\":\"cell\",\"rows\":"<<rows<<",\"cols\":"<<cols<<",\"elems\":[";
        for(size_t i=0;i<n;++i){
            if(i)os<<",";
            const size_t r=(rows>0)?(i%rows):0;
            const size_t c=(rows>0)?(i/rows):0;
            const std::string lbl="{"+std::to_string(r+1)+","+std::to_string(c+1)+"}";
            emitInspectCell(os,lbl,val.cellAt(i));
        }
        os<<"]}"; return os.str();
    }
    os<<"{\"kind\":\"matrix\",\"type\":\""<<numkit::mtypeName(val.type())<<"\""
      <<",\"rows\":"<<d.rows()<<",\"cols\":"<<d.cols();
    if(val.numel()>MATRIX_INSPECT_CAP){os<<",\"truncated\":true}";}
    else{os<<",\"data\":";emitMatrixDataArray(os,val);os<<"}";}
    return os.str();
}

inline bool resolveVar(numkit::Engine& engine, const std::string& name,
                        numkit::Value& out)
{
    if(const numkit::Value* gv=engine.getVariable(name)){out=*gv;return true;}
    return false;
}

} // namespace detail

// ── Public API ────────────────────────────────────────────────────────────────

inline std::string getVarShapeJSON(numkit::Engine& engine, const std::string& name)
{
    try {
        numkit::Value vs;
        if(!detail::resolveVar(engine,name,vs))
            return "{\"error\":\"variable '"+detail::escapeJSON(name)+"' not found\"}";
        const auto& d=vs.dims();
        const size_t rc=d.rows()*d.cols(), pages=(rc>0)?(vs.numel()/rc):1;
        std::ostringstream os;
        os<<"{\"name\":\""<<detail::escapeJSON(name)<<"\""
          <<",\"type\":\""<<numkit::mtypeName(vs.type())<<"\""
          <<",\"rows\":"<<d.rows()<<",\"cols\":"<<d.cols()
          <<",\"ndim\":"<<d.ndim()<<",\"pages\":"<<pages<<",\"dims\":[";
        for(int i=0;i<d.ndim();++i){if(i)os<<",";os<<d.dim(i);}
        os<<"],\"numel\":"<<vs.numel()<<"}";
        return os.str();
    } catch(const std::exception& e){return std::string("{\"error\":\"")+detail::escapeJSON(e.what())+"\"}";}
    catch(...){return "{\"error\":\"unknown\"}";}
}

inline std::string getVarDataJSON(numkit::Engine& engine,
                                  const std::string& name, int page=0)
{
    try {
        numkit::Value vs;
        if(!detail::resolveVar(engine,name,vs))
            return "{\"error\":\"variable '"+detail::escapeJSON(name)+"' not found\"}";
        const auto& d=vs.dims();
        const size_t rc=d.rows()*d.cols(), pages=(rc>0)?(vs.numel()/rc):1;
        if(page<0)page=0;
        if(pages>0&&(size_t)page>=pages)page=(int)pages-1;
        std::ostringstream os;
        os<<"{\"name\":\""<<detail::escapeJSON(name)<<"\""
          <<",\"type\":\""<<numkit::mtypeName(vs.type())<<"\""
          <<",\"rows\":"<<d.rows()<<",\"cols\":"<<d.cols()
          <<",\"page\":"<<page<<",\"pages\":"<<pages<<",\"data\":";
        detail::emitMatrixDataArray(os,vs,(size_t)page);
        os<<"}"; return os.str();
    } catch(const std::exception& e){return std::string("{\"error\":\"")+detail::escapeJSON(e.what())+"\"}";}
    catch(...){return "{\"error\":\"unknown\"}";}
}

inline std::string getVarTileJSON(numkit::Engine& engine, const std::string& name,
                                  int r0, int c0, int rowsIn, int colsIn, int page=0)
{
    try {
        using numkit::ValueType;
        numkit::Value vs;
        if(!detail::resolveVar(engine,name,vs)) return "{\"error\":\"variable not found\"}";
        const auto& d=vs.dims();
        const size_t totalRows=d.rows(),totalCols=d.cols();
        const size_t rc=totalRows*totalCols,pages=(rc>0)?(vs.numel()/rc):1;
        if(page<0)page=0;
        if(pages>0&&(size_t)page>=pages)page=(int)pages-1;
        const size_t pageOff=(size_t)page*rc;
        if(r0<0)r0=0; if(c0<0)c0=0;
        const size_t rEnd=std::min(totalRows,(size_t)(r0+rowsIn));
        const size_t cEnd=std::min(totalCols,(size_t)(c0+colsIn));
        if((size_t)r0>=totalRows||(size_t)c0>=totalCols||rEnd<=(size_t)r0||cEnd<=(size_t)c0)
            return "{\"error\":\"out of range\",\"r0\":"+std::to_string(r0)+",\"c0\":"+std::to_string(c0)+"}";
        std::ostringstream os;
        os<<"{\"r0\":"<<r0<<",\"c0\":"<<c0<<",\"rows\":"<<(rEnd-r0)<<",\"cols\":"<<(cEnd-c0)
          <<",\"page\":"<<page<<",\"type\":\""<<numkit::mtypeName(vs.type())<<"\",\"data\":[";
        if(numkit::isRealNumericCell(vs.type())){
            for(size_t r=(size_t)r0;r<rEnd;++r){if(r>(size_t)r0)os<<",";os<<"[";
                for(size_t c=(size_t)c0;c<cEnd;++c){if(c>(size_t)c0)os<<",";os<<numkit::numericCellJSON(vs,pageOff+c*totalRows+r);}
                os<<"]";}
        } else if(vs.type()==ValueType::COMPLEX){
            const numkit::Complex* p=vs.complexData();
            for(size_t r=(size_t)r0;r<rEnd;++r){if(r>(size_t)r0)os<<",";os<<"[";
                for(size_t c=(size_t)c0;c<cEnd;++c){if(c>(size_t)c0)os<<",";
                    const auto& z=p[pageOff+c*totalRows+r];
                    std::ostringstream s;s.precision(12);s<<z.real();if(z.imag()>=0)s<<"+";s<<z.imag()<<"i";
                    os<<"\""<<s.str()<<"\"";}
                os<<"]";}
        } else if(vs.type()==ValueType::CHAR){
            const char* p=vs.charData();
            for(size_t r=(size_t)r0;r<rEnd;++r){if(r>(size_t)r0)os<<",";os<<"[";
                for(size_t c=(size_t)c0;c<cEnd;++c){if(c>(size_t)c0)os<<",";
                    char ch=p[pageOff+c*totalRows+r];
                    os<<"\""<<detail::escapeJSON(std::string(1,ch))<<"\"";}
                os<<"]";}
        } else {
            for(size_t r=(size_t)r0;r<rEnd;++r){if(r>(size_t)r0)os<<",";os<<"[";
                for(size_t c=(size_t)c0;c<cEnd;++c){if(c>(size_t)c0)os<<",";os<<"\"\\u2014\"";}
                os<<"]";}
        }
        os<<"]}"; return os.str();
    } catch(const std::exception& e){return std::string("{\"error\":\"")+detail::escapeJSON(e.what())+"\"}";}
    catch(...){return "{\"error\":\"unknown\"}";}
}

inline std::string getVarStatsJSON(numkit::Engine& engine,
                                   const std::string& name, int page=-1)
{
    try {
        using numkit::ValueType;
        numkit::Value vs;
        if(!detail::resolveVar(engine,name,vs)) return "{\"error\":\"variable not found\"}";
        const auto& d=vs.dims();
        const size_t totalRows=d.rows(),totalCols=d.cols(),numel=vs.numel();
        const size_t rc=totalRows*totalCols,pages=(rc>0)?(numel/rc):1;
        size_t i0=0,i1=numel;
        if(page>=0&&(size_t)page<pages){i0=(size_t)page*rc;i1=i0+rc;}
        double mn=std::numeric_limits<double>::infinity();
        double mx=-std::numeric_limits<double>::infinity();
        double sum=0.0; size_t n=0; bool hasNaN=false;
        if(vs.type()==ValueType::DOUBLE){
            const double* p=vs.doubleData();
            for(size_t i=i0;i<i1;++i){double v=p[i];if(std::isnan(v)){hasNaN=true;continue;}
                if(!std::isfinite(v))continue;if(v<mn)mn=v;if(v>mx)mx=v;sum+=v;++n;}
        } else if(vs.type()==ValueType::LOGICAL){
            const uint8_t* p=vs.logicalData();
            for(size_t i=i0;i<i1;++i){double v=p[i]?1.0:0.0;if(v<mn)mn=v;if(v>mx)mx=v;sum+=v;++n;}
        } else if(vs.type()==ValueType::COMPLEX){
            const numkit::Complex* p=vs.complexData();
            for(size_t i=i0;i<i1;++i){double mag=std::hypot(p[i].real(),p[i].imag());
                if(std::isnan(mag)){hasNaN=true;continue;}if(!std::isfinite(mag))continue;
                if(mag<mn)mn=mag;if(mag>mx)mx=mag;sum+=mag;++n;}
        } else if(numkit::isFloatType(vs.type())||numkit::isIntegerType(vs.type())){
            for(size_t i=i0;i<i1;++i){double v=vs.elemAsDouble(i);if(std::isnan(v)){hasNaN=true;continue;}
                if(!std::isfinite(v))continue;if(v<mn)mn=v;if(v>mx)mx=v;sum+=v;++n;}
        } else { return "{\"error\":\"non-numeric type\"}"; }
        std::ostringstream os; os.precision(17);
        os<<"{\"rows\":"<<totalRows<<",\"cols\":"<<totalCols<<",\"n\":"<<n
          <<",\"hasNaN\":"<<(hasNaN?"true":"false");
        if(n>0){
            os<<",\"min\":"<<mn<<",\"max\":"<<mx<<",\"mean\":"<<(sum/n);
            numkit::ValueStats fs;
            if(numkit::computeValueStatsRange(vs,i0,i1-i0,fs))
                os<<",\"median\":"<<fs.median<<",\"mode\":"<<fs.mode
                  <<",\"var\":"<<fs.var<<",\"std\":"<<fs.std;
        } else { os<<",\"min\":null,\"max\":null,\"mean\":null"; }
        os<<"}"; return os.str();
    } catch(const std::exception& e){return std::string("{\"error\":\"")+detail::escapeJSON(e.what())+"\"}";}
    catch(...){return "{\"error\":\"unknown\"}";}
}

inline std::string getInspectPathJSON(numkit::Engine& engine,
                                      const std::string& name,
                                      const std::string& pathStr)
{
    try {
        numkit::Value rootStore;
        if(!detail::resolveVar(engine,name,rootStore))
            return "{\"error\":\"variable '"+detail::escapeJSON(name)+"' not found\"}";
        auto steps=detail::parseInspectPath(pathStr);
        std::vector<numkit::Value> owned;
        const numkit::Value* cur=detail::resolveInspectPath(rootStore,steps,owned);
        if(!cur) return "{\"error\":\"invalid path\"}";
        return detail::emitInspectPayload(*cur);
    } catch(const std::exception& e){return std::string("{\"error\":\"")+detail::escapeJSON(e.what())+"\"}";}
    catch(...){return "{\"error\":\"unknown\"}";}
}

inline std::string getVarFigureJSON(numkit::Engine& engine,
                                    const std::string& name,
                                    const std::string& optsJSON)
{
    try {
        numkit::Value rootStore;
        if(!detail::resolveVar(engine,name,rootStore))
            return "{\"error\":\"variable '"+detail::escapeJSON(name)+"' not found\"}";
        
        bool isSpy = optsJSON.find("\"mode\":\"spy\"") != std::string::npos;
        bool isSurf = optsJSON.find("\"mode\":\"surf\"") != std::string::npos || optsJSON.find("\"mode\":\"mesh\"") != std::string::npos;
        bool isContour = optsJSON.find("\"mode\":\"contour\"") != std::string::npos;
        
        const auto& d = rootStore.dims();
        size_t rows = d.rows(), cols = d.cols();
        if (rows == 0 || cols == 0 || !rootStore.isNumeric()) {
             return "{\"error\":\"invalid data for figure\"}";
        }
        
        std::ostringstream os; os.precision(5);
        if (isSpy) {
            size_t maxPts = 50000;
            os << "{\"kind\":\"composite\",\"id\":\"fig_" << name << "\",\"title\":\"" << name << " spy\",\"xLabel\":\"col\",\"yLabel\":\"row\",";
            os << "\"xRange\":[0," << cols << "],\"yRange\":[0," << rows << "],\"yDir\":\"reverse\",\"layers\":[{";
            os << "\"kind\":\"series\",\"mode\":\"scatter\",\"name\":\"non-zeros\",\"marker\":\".\",\"x\":[";
            std::vector<size_t> xs, ys;
            size_t added = 0;
            for(size_t c=0; c<cols && added<maxPts; c++) {
                for(size_t r=0; r<rows && added<maxPts; r++) {
                    double v = rootStore.elemAsDouble(r + c*rows);
                    if (v != 0.0 && !std::isnan(v)) {
                        xs.push_back(c + 1);
                        ys.push_back(r + 1);
                        added++;
                    }
                }
            }
            for(size_t i=0; i<xs.size(); ++i) { if(i) os << ","; os << xs[i]; }
            os << "],\"y\":[";
            for(size_t i=0; i<ys.size(); ++i) { if(i) os << ","; os << ys[i]; }
            os << "]}]}";
        } else if (isSurf) {
            size_t rStep = std::max<size_t>(1, rows / 60);
            size_t cStep = std::max<size_t>(1, cols / 60);
            size_t outRows = (rows + rStep - 1) / rStep;
            size_t outCols = (cols + cStep - 1) / cStep;
            
            os << "{\"kind\":\"composite3d\",\"id\":\"fig_" << name << "\",\"title\":\"" << name << "\",\"xLabel\":\"col\",\"yLabel\":\"row\",\"zLabel\":\"val\",";
            os << "\"xRange\":[1," << cols << "],\"yRange\":[1," << rows << "],\"layers\":[{";
            os << "\"kind\":\"series\",\"mode\":\"" << (optsJSON.find("\"mode\":\"mesh\"") != std::string::npos ? "mesh" : "surface") << "\",\"name\":\"" << name << "\",\"surfaceGrid\":{";
            os << "\"Xs\":[";
            for(size_t c=0; c<outCols; ++c) { if(c>0) os<<","; os << (c*cStep+1); }
            os << "],\"Ys\":[";
            for(size_t r=0; r<outRows; ++r) { if(r>0) os<<","; os << (r*rStep+1); }
            os << "],\"Z\":[";
            for(size_t c=0; c<outCols; ++c) {
                if (c>0) os << ",";
                os << "[";
                size_t srcC = c * cStep;
                for(size_t r=0; r<outRows; ++r) {
                    if (r>0) os << ",";
                    size_t srcR = r * rStep;
                    double v = rootStore.elemAsDouble(srcR + srcC*rows);
                    if (std::isnan(v)) os << "null";
                    else os << v;
                }
                os << "]";
            }
            os << "]}}]}";
        } else {
            size_t rStep = std::max<size_t>(1, rows / 250);
            size_t cStep = std::max<size_t>(1, cols / 250);
            size_t outRows = (rows + rStep - 1) / rStep;
            size_t outCols = (cols + cStep - 1) / cStep;
            
            os << "{\"kind\":\"composite\",\"id\":\"fig_" << name << "\",\"title\":\"" << name << "\",\"xLabel\":\"col\",\"yLabel\":\"row\",";
            os << "\"xRange\":[1," << cols << "],\"yRange\":[1," << rows << "],\"yDir\":\"reverse\",\"layers\":[{";
            if (isContour) {
                os << "\"kind\":\"series\",\"mode\":\"contour\",\"name\":\"" << name << "\",\"surfaceGrid\":{";
                os << "\"Xs\":[";
                for(size_t c=0; c<outCols; ++c) { if(c>0) os<<","; os << (c*cStep+1); }
                os << "],\"Ys\":[";
                for(size_t r=0; r<outRows; ++r) { if(r>0) os<<","; os << (r*rStep+1); }
                os << "],\"Z\":[";
                for(size_t c=0; c<outCols; ++c) {
                    if (c>0) os << ",";
                    os << "[";
                    size_t srcC = c * cStep;
                    for(size_t r=0; r<outRows; ++r) {
                        if (r>0) os << ",";
                        size_t srcR = r * rStep;
                        double v = rootStore.elemAsDouble(srcR + srcC*rows);
                        if (std::isnan(v)) os << "null";
                        else os << v;
                    }
                    os << "]";
                }
                os << "]}}]}";
            } else {
                double cmin = std::numeric_limits<double>::infinity();
                double cmax = -std::numeric_limits<double>::infinity();
                for(size_t r=0; r<outRows; ++r) {
                    size_t srcR = r * rStep;
                    for(size_t c=0; c<outCols; ++c) {
                        size_t srcC = c * cStep;
                        double v = rootStore.elemAsDouble(srcR + srcC*rows);
                        if(std::isfinite(v)) {
                            if (v < cmin) cmin = v;
                            if (v > cmax) cmax = v;
                        }
                    }
                }
                if(cmin > cmax) { cmin = 0; cmax = 1; }
                if(cmin == cmax) { cmax = cmin + 1; }

                os << "\"kind\":\"heatmap\",\"name\":\"" << name << "\",\"cminOrig\":" << cmin << ",\"cmaxOrig\":" << cmax << ",\"originalRows\":" << rows << ",\"originalCols\":" << cols << ",\"z\":[";
                for(size_t r=0; r<outRows; ++r) {
                    if (r>0) os << ",";
                    os << "[";
                    size_t srcR = r * rStep;
                    for(size_t c=0; c<outCols; ++c) {
                        if (c>0) os << ",";
                        size_t srcC = c * cStep;
                        double v = rootStore.elemAsDouble(srcR + srcC*rows);
                        if (std::isnan(v)) {
                            os << "null";
                        } else {
                            int idx = 0;
                            if (v >= cmax) idx = 255;
                            else if (v > cmin) idx = static_cast<int>(255.0 * (v - cmin) / (cmax - cmin));
                            os << idx;
                        }
                    }
                    os << "]";
                }
                os << "]}]}";
            }
        }
        return os.str();
    } catch(const std::exception& e){return std::string("{\"error\":\"")+detail::escapeJSON(e.what())+"\"}";}
    catch(...){return "{\"error\":\"unknown\"}";}
}

// Parse tab-separated params from a protocol line after the prefix.
// e.g. "__GET_TILE__:A\t0\t0\t50\t50\t0" with prefixLen=14 -> ["A","0","0","50","50","0"]
inline std::vector<std::string> parseTabParams(const std::string& line, size_t prefixLen)
{
    const std::string rest=line.substr(prefixLen);
    std::vector<std::string> tokens;
    size_t i=0;
    while(true){
        size_t tab=rest.find('\t',i);
        tokens.push_back(rest.substr(i,tab==std::string::npos?std::string::npos:tab-i));
        if(tab==std::string::npos)break;
        i=tab+1;
    }
    return tokens;
}

}} // namespace numkit::ide
