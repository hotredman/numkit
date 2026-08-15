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
                        numkit::Value& out, numkit::DebugSession* session = nullptr)
{
    if (session && session->isActive()) {
        auto snap = session->snapshot();
        for (auto &v : snap.variables) {
            if (v.name == name && v.value) {
                out = *v.value;
                return true;
            }
        }
    }
    if (const numkit::Value* gv = engine.getVariable(name)) {
        out = *gv;
        return true;
    }
    return false;
}

} // namespace detail

// ── Public API ────────────────────────────────────────────────────────────────

inline std::string getVarShapeJSON(numkit::Engine& engine, const std::string& name,
                                   numkit::DebugSession* session = nullptr)
{
    try {
        numkit::Value vs;
        if(!detail::resolveVar(engine,name,vs,session))
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
                                  const std::string& name, int page=0,
                                  numkit::DebugSession* session = nullptr)
{
    try {
        numkit::Value vs;
        if(!detail::resolveVar(engine,name,vs,session))
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
                                  int r0, int c0, int rowsIn, int colsIn, int page=0,
                                  numkit::DebugSession* session = nullptr)
{
    try {
        using numkit::ValueType;
        numkit::Value vs;
        if(!detail::resolveVar(engine,name,vs,session)) return "{\"error\":\"variable not found\"}";
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
                                   const std::string& name, int page=-1,
                                   numkit::DebugSession* session = nullptr)
{
    try {
        using numkit::ValueType;
        numkit::Value vs;
        if(!detail::resolveVar(engine,name,vs,session)) return "{\"error\":\"variable not found\"}";
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
                                      const std::string& pathStr,
                                      numkit::DebugSession* session = nullptr)
{
    try {
        numkit::Value rootStore;
        if(!detail::resolveVar(engine,name,rootStore,session))
            return "{\"error\":\"variable '"+detail::escapeJSON(name)+"' not found\"}";
        auto steps=detail::parseInspectPath(pathStr);
        std::vector<numkit::Value> owned;
        const numkit::Value* cur=detail::resolveInspectPath(rootStore,steps,owned);
        if(!cur) return "{\"error\":\"invalid path\"}";
        return detail::emitInspectPayload(*cur);
    } catch(const std::exception& e){return std::string("{\"error\":\"")+detail::escapeJSON(e.what())+"\"}";}
    catch(...){return "{\"error\":\"unknown\"}";}
}

namespace detail {

inline std::string extractJsonString(const std::string& json, const std::string& key, const std::string& defVal = "")
{
    std::string needle = "\"" + key + "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return defVal;
    pos += needle.size();
    auto end = json.find('"', pos);
    if (end == std::string::npos) return defVal;
    return json.substr(pos, end - pos);
}

inline int extractJsonInt(const std::string& json, const std::string& key, int defVal = 0)
{
    std::string needle = "\"" + key + "\":";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return defVal;
    pos += needle.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    try {
        return std::stoi(json.substr(pos));
    } catch (...) {
        return defVal;
    }
}

inline std::vector<int> extractJsonIntArray(const std::string& json, const std::string& key)
{
    std::vector<int> res;
    std::string needle = "\"" + key + "\":[";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return res;
    pos += needle.size();
    auto end = json.find(']', pos);
    if (end == std::string::npos) return res;
    std::string body = json.substr(pos, end - pos);
    std::stringstream ss(body);
    std::string token;
    while (std::getline(ss, token, ',')) {
        try {
            while (!token.empty() && (token.front() == ' ' || token.front() == '\t')) token.erase(token.begin());
            while (!token.empty() && (token.back() == ' ' || token.back() == '\t')) token.pop_back();
            if (!token.empty()) res.push_back(std::stoi(token));
        } catch (...) {}
    }
    return res;
}

} // namespace detail

inline std::string getVarFigureJSON(numkit::Engine& engine,
                                    const std::string& name,
                                    const std::string& optsJSON,
                                    numkit::DebugSession* session = nullptr)
{
    try {
        numkit::Value rootStore;
        if(!detail::resolveVar(engine,name,rootStore,session))
            return "{\"error\":\"variable '"+detail::escapeJSON(name)+"' not found\"}";
        
        std::string dimMode = detail::extractJsonString(optsJSON, "dimMode", "");
        std::string mode = detail::extractJsonString(optsJSON, "mode", "");
        bool isSpy = (mode == "spy") || optsJSON.find("\"mode\":\"spy\"") != std::string::npos;
        bool isSurf = (mode == "surf" || mode == "mesh") || optsJSON.find("\"mode\":\"surf\"") != std::string::npos || optsJSON.find("\"mode\":\"mesh\"") != std::string::npos;
        bool isContour = (mode == "contour") || optsJSON.find("\"mode\":\"contour\"") != std::string::npos;
        bool isImagesc = (mode == "imagesc") || optsJSON.find("\"mode\":\"imagesc\"") != std::string::npos;

        const auto& d = rootStore.dims();
        size_t rows = d.rows(), cols = d.cols();
        if (rows == 0 || cols == 0 || !rootStore.isNumeric()) {
             return "{\"error\":\"invalid data for figure\"}";
        }

        bool is1D = (dimMode == "1d") || (!isSpy && !isSurf && !isContour && !isImagesc &&
                    dimMode != "2d" && dimMode != "3d" &&
                    (mode == "line" || mode == "stem" || mode == "bar" || mode == "scatter" ||
                     mode == "area" || mode == "stairs" || rows == 1 || cols == 1));

        std::ostringstream os; os.precision(5);
        if (is1D) {
            std::string plotMode = mode.empty() ? "line" : mode;
            if (plotMode != "line" && plotMode != "stem" && plotMode != "bar" &&
                plotMode != "scatter" && plotMode != "area" && plotMode != "stairs") {
                plotMode = "line";
            }
            std::string axis = detail::extractJsonString(optsJSON, "axis", (cols >= rows) ? "row" : "col");
            std::vector<int> indices = detail::extractJsonIntArray(optsJSON, "indices");
            if (indices.empty()) {
                int singleIdx = detail::extractJsonInt(optsJSON, "idx", 0);
                indices.push_back(singleIdx);
            }

            std::string xMode = detail::extractJsonString(optsJSON, "xMode", "index");
            std::string xSrcAxis = axis;
            int xSrcIdx = 0;
            size_t xSrcPos = optsJSON.find("\"xSrc\":");
            if (xSrcPos != std::string::npos) {
                std::string xSrcSub = optsJSON.substr(xSrcPos);
                xSrcAxis = detail::extractJsonString(xSrcSub, "axis", axis);
                xSrcIdx = detail::extractJsonInt(xSrcSub, "idx", 0);
            }
            int page = detail::extractJsonInt(optsJSON, "page", 0);
            size_t totalRows = rows, totalCols = cols, numel = rootStore.numel();
            size_t rc = totalRows * totalCols, pages = (rc > 0) ? (numel / rc) : 1;
            if (page < 0) page = 0;
            if (pages > 0 && (size_t)page >= pages) page = (int)pages - 1;
            size_t pageOff = (size_t)page * rc;

            size_t sliceLen = (axis == "row") ? totalCols : totalRows;
            if (sliceLen == 0) return "{\"error\":\"empty slice\"}";

            static const char* palette[] = {
                "#7fd99a", "#5fb3d4", "#e9b870", "#9b8cf2",
                "#e26a6a", "#d4a5e6", "#f2a37e", "#6fcfbf"
            };

            double globalXMin = std::numeric_limits<double>::infinity();
            double globalXMax = -std::numeric_limits<double>::infinity();
            double globalYMin = std::numeric_limits<double>::infinity();
            double globalYMax = -std::numeric_limits<double>::infinity();

            struct CurveData {
                std::string name;
                std::string color;
                std::vector<double> x;
                std::vector<double> y;
            };
            std::vector<CurveData> curves;

            for (size_t curveI = 0; curveI < indices.size(); ++curveI) {
                int idx = indices[curveI];
                if (idx < 0) idx = 0;
                if (axis == "row" && (size_t)idx >= totalRows) idx = (int)totalRows - 1;
                if (axis == "col" && (size_t)idx >= totalCols) idx = (int)totalCols - 1;

                CurveData c;
                c.name = axis + " " + std::to_string(idx + 1);
                c.color = palette[curveI % 8];

                auto getY = [&](size_t i) -> double {
                    size_t off = (axis == "row") ? (pageOff + i * totalRows + idx)
                                                 : (pageOff + (size_t)idx * totalRows + i);
                    return rootStore.elemAsDouble(off);
                };

                auto getX = [&](size_t i) -> double {
                    if (xMode == "src") {
                        size_t off = (xSrcAxis == "row") ? (pageOff + i * totalRows + xSrcIdx)
                                                         : (pageOff + (size_t)xSrcIdx * totalRows + i);
                        return rootStore.elemAsDouble(off);
                    }
                    return static_cast<double>(i + 1);
                };

                if (sliceLen <= 4000) {
                    c.x.reserve(sliceLen);
                    c.y.reserve(sliceLen);
                    for (size_t i = 0; i < sliceLen; ++i) {
                        double xv = getX(i);
                        double yv = getY(i);
                        if (std::isfinite(xv) && std::isfinite(yv)) {
                            c.x.push_back(xv);
                            c.y.push_back(yv);
                            if (xv < globalXMin) globalXMin = xv;
                            if (xv > globalXMax) globalXMax = xv;
                            if (yv < globalYMin) globalYMin = yv;
                            if (yv > globalYMax) globalYMax = yv;
                        }
                    }
                } else {
                    size_t B = 1000;
                    c.x.reserve(B * 4);
                    c.y.reserve(B * 4);
                    for (size_t b = 0; b < B; ++b) {
                        size_t i0 = b * sliceLen / B;
                        size_t i1 = (b + 1) * sliceLen / B;
                        if (i1 <= i0) continue;
                        size_t first = i0;
                        size_t last = i1 - 1;
                        size_t minI = i0, maxI = i0;
                        double minV = getY(i0);
                        double maxV = minV;
                        for (size_t i = i0; i < i1; ++i) {
                            double v = getY(i);
                            if (std::isfinite(v)) {
                                if (!std::isfinite(minV) || v < minV) { minV = v; minI = i; }
                                if (!std::isfinite(maxV) || v > maxV) { maxV = v; maxI = i; }
                            }
                        }
                        std::vector<size_t> pts = {first, minI, maxI, last};
                        std::sort(pts.begin(), pts.end());
                        pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
                        for (size_t pi : pts) {
                            double xv = getX(pi);
                            double yv = getY(pi);
                            if (std::isfinite(xv) && std::isfinite(yv)) {
                                c.x.push_back(xv);
                                c.y.push_back(yv);
                                if (xv < globalXMin) globalXMin = xv;
                                if (xv > globalXMax) globalXMax = xv;
                                if (yv < globalYMin) globalYMin = yv;
                                if (yv > globalYMax) globalYMax = yv;
                            }
                        }
                    }
                }
                curves.push_back(std::move(c));
            }

            if (!std::isfinite(globalXMin) || !std::isfinite(globalXMax)) { globalXMin = -1; globalXMax = 1; }
            if (!std::isfinite(globalYMin) || !std::isfinite(globalYMax)) { globalYMin = -1; globalYMax = 1; }
            if (globalXMin == globalXMax) { globalXMin -= 0.5; globalXMax += 0.5; }
            if (globalYMin == globalYMax) { globalYMin -= 0.5; globalYMax += 0.5; }

            std::string titleStr = name + " (" + axis + " ";
            for (size_t i = 0; i < indices.size(); ++i) {
                if (i > 0) titleStr += ", ";
                titleStr += std::to_string(indices[i] + 1);
            }
            titleStr += ")";
            std::string xLabelStr = (xMode == "src") ? (xSrcAxis + " " + std::to_string(xSrcIdx + 1)) : "index";

            os << "{\"kind\":\"composite\",\"id\":\"fig_" << detail::escapeJSON(name) << "\",\"title\":\"" << detail::escapeJSON(titleStr) << "\",\"xLabel\":\"" << detail::escapeJSON(xLabelStr) << "\",\"yLabel\":\"\",";
            os << "\"xRange\":[" << globalXMin << "," << globalXMax << "],\"yRange\":[" << globalYMin << "," << globalYMax << "],\"grid\":true,\"layers\":[";
            for (size_t ci = 0; ci < curves.size(); ++ci) {
                const auto& c = curves[ci];
                if (ci > 0) os << ",";
                os << "{\"kind\":\"series\",\"mode\":\"" << detail::escapeJSON(plotMode) << "\",\"name\":\"" << detail::escapeJSON(c.name) << "\",\"color\":\"" << c.color << "\",\"x\":[";
                for (size_t i = 0; i < c.x.size(); ++i) {
                    if (i > 0) os << ",";
                    os << c.x[i];
                }
                os << "],\"y\":[";
                for (size_t i = 0; i < c.y.size(); ++i) {
                    if (i > 0) os << ",";
                    os << c.y[i];
                }
                os << "]}";
            }
            os << "]}";
            return os.str();
        } else if (isSpy) {
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
