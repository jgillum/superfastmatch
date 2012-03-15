#include "api.h"

namespace superfastmatch{
  
	RegisterTemplateFilename(SUCCESS_JSON, "JSON/success.tpl");
	RegisterTemplateFilename(FAILURE_JSON, "JSON/failure.tpl");
	RegisterTemplateFilename(DESCRIPTION_HTML, "HTML/description.tpl");

  // -------------------  
  // ApiResponse members
  // -------------------
  
  ApiResponse::ApiResponse():
  type(-1,"text/html"),
  dict("response")
  {}
    
  // -------------------  
  // ApiParams members
  // -------------------
    
  ApiParams::ApiParams(const HTTPClient::Method verb,const string& body, const string& querystring):
  body(body)
  {
    if (verb==HTTPClient::MPOST || verb==HTTPClient::MPUT){
      wwwformtomap(body,&form);
    }
    vector<string> queries,parts;
    kc::strsplit(querystring,"&",&queries);
    for (vector<string>::const_iterator it=queries.begin();it!=queries.end();++it){
      kc::strsplit(*it,"=",&parts);
      if (parts.size()==2){
        size_t ksiz,vsiz;
        char* kbuf = kc::urldecode(parts[0].c_str(), &ksiz);
        char* vbuf = kc::urldecode(parts[1].c_str(), &vsiz);
        query[kbuf]=vbuf;
        delete[] kbuf;
        delete[] vbuf;
      }
    }  
  }

  // -------------------
  // ApiCall members
  // -------------------
  
  ApiCall::ApiCall(const HTTPClient::Method verb,
                   const char* match,
                   const response_map& responses,
                   const char* description,
                   const ApiMethod method):
    verb(verb),
    match(match),
    responses(responses),
    description(description),
    method(method){}
    
  // -------------------
  // Api members
  // -------------------
    
  const size_t Api::METHOD_COUNT=6;
    
  Api::Api(Registry* registry):
    registry_(registry)
  {
    for (size_t i=0;i<Api::METHOD_COUNT;i++){
      matchers_.push_back(MatcherPtr(new Matcher()));
    }
    for (size_t i=0;i<Api::API_COUNT;i++){
      MatcherPtr matcher=matchers_[calls_[i].verb];
      int atom_id;
      string match=calls_[i].match;
      for (capture_map::const_iterator it=captures_.begin(),ite=captures_.end();it!=ite;++it){
        Replace(match,it->first,it->second.first);
      }
      assert(matcher->f.Add(match,matcher->options,&atom_id)==re2::RE2::NoError);
      matcher->atom_indices.push_back(atom_id);
      matcher->regexes[atom_id]=RE2Ptr(new re2::RE2(match));
      matcher->calls[atom_id]=&calls_[i];
    }
    for(size_t i=0;i<METHOD_COUNT;i++){
      if (matchers_[i]->f.NumRegexps()>0){
        matchers_[i]->f.Compile(&matchers_[i]->atoms); 
      }
    }
  }

  int Api::Invoke(const string& path, HTTPClient::Method verb,
                   const map<string, string>& reqheads,
                   const string& reqbody,
                   map<string, string>& resheads,
                   string& resbody,
                   const map<string, string>& misc)
  {
    ApiParams params(verb,reqbody,misc.find("query")->second);
    ApiResponse response;
    string lowercase_path(path);
    kc::strtolower(&lowercase_path);
    MatcherPtr matcher=matchers_[verb];
    int id=MatchApiCall(matcher,lowercase_path,params);
    if(id!=-1){
      (this->*matcher->calls[id]->method)(params,response);
      map<response_t,string>::const_iterator it=matcher->calls[id]->responses.find(response.type);
      if (it!=matcher->calls[id]->responses.end()){
        registry_->getTemplateCache()->ExpandWithData(it->second,STRIP_BLANK_LINES,&response.dict,NULL,&resbody); 
      }
      resheads["content-type"] = response.type.second;
    }
    return response.type.first;
  } 
  
  int Api::MatchApiCall(MatcherPtr matcher,const string& path,ApiParams& params){
    int id=matcher->f.FirstMatch(path,matcher->atom_indices);
    if (id!=-1){
      RE2Ptr regex=matcher->regexes[id];
      string* captures=new string[regex->NumberOfCapturingGroups()];
      RE2::Arg* argv=new RE2::Arg[regex->NumberOfCapturingGroups()];
      RE2::Arg** args=new RE2::Arg*[regex->NumberOfCapturingGroups()];
      for(int i=0;i<regex->NumberOfCapturingGroups();i++){
        argv[i]=&captures[i];
        args[i]=&argv[i];
      };
      if (re2::RE2::FullMatchN(path,*regex,args,regex->NumberOfCapturingGroups())){
        for (map<string,int>::const_iterator it=regex->NamedCapturingGroups().begin(),ite=regex->NamedCapturingGroups().end();it!=ite;++it){
          params.resource[it->first]=captures[it->second-1];
        }
        for (map<string,int>::const_iterator it=regex->NamedCapturingGroups().begin(),ite=regex->NamedCapturingGroups().end();it!=ite;++it){
        
        }
      }
      delete[] args;
      delete[] argv;
      delete[] captures;
    }
    return id;
  }

  // --------------------
  // Api call definitions
  // --------------------

  const size_t Api::API_COUNT=19;

  const capture_map Api::captures_=create_map<string,capture_t >\
                                   ("<doctype>",capture_t("(?P<doctype>\\d+)","A document type with a value greater than 0"))\
                                   ("<docid>",capture_t("(?P<docid>\\d+)","A document id with a value greater than 0"))\
                                   ("<source>",capture_t("(?P<source>(((\\d+-\\d+):?|(\\d+):?))+)","A range of doctypes. Eg. 1-2:5:7-9 which is equivalent to [1,2,5,7,8,9]"))\
                                   ("<target>",capture_t("(?P<target>(((\\d+-\\d+):?|(\\d+):?))+)","A range of doctypes. Eg. 1-2:5:7-9 which is equivalent to [1,2,5,7,8,9]")); 

  const ApiCall Api::calls_[Api::API_COUNT]={
    ApiCall(HTTPClient::MPOST,
            "^/search/?$",
            create_map<response_t,string>(response_t(200,"application/json"),SUCCESS_JSON)\
                                         (response_t(500,"application/json"),SUCCESS_JSON),  
            "Search for text in all documents",
            &Api::DoSearch),
    ApiCall(HTTPClient::MPOST,
            "^/search/<target>/?$",
            create_map<response_t,string>(response_t(200,"application/json"),SUCCESS_JSON)\
                                         (response_t(500,"application/json"),SUCCESS_JSON),  
            "Search for text in the specified target doctype range",
            &Api::DoSearch),                        
    ApiCall(HTTPClient::MGET,
            "^/document/?$",
            create_map<response_t,string>(response_t(200,"application/json"),SUCCESS_JSON)\
                                         (response_t(500,"application/json"),FAILURE_JSON),
            "Get metadata and text of all documents",
            &Api::GetDocuments),
    ApiCall(HTTPClient::MGET,
            "^/document/<source>/?$",
            create_map<response_t,string>(response_t(200,"application/json"),SUCCESS_JSON)\
                                         (response_t(500,"application/json"),FAILURE_JSON),
            "Get metadata and text of all documents with specified doctype",
            &Api::GetDocuments),
    ApiCall(HTTPClient::MGET,
            "^/document/<doctype>/<docid>/?$",
            create_map<response_t,string>(response_t(200,"application/json"),SUCCESS_JSON)\
                                         (response_t(404,"application/json"),FAILURE_JSON),
            "Get metadata and text of existing document",
            &Api::GetDocument),
    ApiCall(HTTPClient::MPOST,
           "^/document/<doctype>/<docid>/?$",
           create_map<response_t,string>(response_t(202,"application/json"),SUCCESS_JSON)\
                                        (response_t(500,"application/json"),FAILURE_JSON),
           "Create a new document",
           &Api::CreateDocument),
    ApiCall(HTTPClient::MPUT,
           "^/document/<doctype>/<docid>/?$",
           create_map<response_t,string>(response_t(202,"application/json"),SUCCESS_JSON)\
                                        (response_t(500,"application/json"),FAILURE_JSON),
           "Create and associate a new document",
           &Api::CreateAndAssociateDocument),
    ApiCall(HTTPClient::MDELETE,
           "^/document/<doctype>/<docid>/?$",
           create_map<response_t,string>(response_t(202,"application/json"),SUCCESS_JSON),
           "Delete a document",
           &Api::DeleteDocument),
    ApiCall(HTTPClient::MPOST,
          "^/association/<doctype>/<docid>/?$",
          create_map<response_t,string>(response_t(202,"application/json"),SUCCESS_JSON),
          "Associate a document",
          &Api::AssociateDocument),
    ApiCall(HTTPClient::MPOST,
          "^/association/<doctype>/<docid>/<target>/?$",
          create_map<response_t,string>(response_t(202,"application/json"),SUCCESS_JSON),
          "Associate a document with a set of documents that match the specified target doc type range",
          &Api::AssociateDocument),
    ApiCall(HTTPClient::MPOST,
          "^/associations/?$",
          create_map<response_t,string>(response_t(202,"application/json"),SUCCESS_JSON),
          "Associate all documents",
          &Api::AssociateDocuments),   
    ApiCall(HTTPClient::MPOST,
          "^/associations/<source>/?$",
          create_map<response_t,string>(response_t(202,"application/json"),SUCCESS_JSON),
          "Associate a set of documents which match the specified source doc type range",
          &Api::AssociateDocuments),
    ApiCall(HTTPClient::MPOST,
          "^/associations/<source>/<target>/?$",
          create_map<response_t,string>(response_t(202,"application/json"),SUCCESS_JSON),
          "Associate a set of documents which match the specified source doc type range with the target doc type range",
          &Api::AssociateDocuments),
    ApiCall(HTTPClient::MGET,
           "^/index/?$",
           create_map<response_t,string>(response_t(200,"application/json"),SUCCESS_JSON),
           "Get the index data",
           &Api::GetIndex),
    ApiCall(HTTPClient::MGET,
           "^/queue/?$",
           create_map<response_t,string>(response_t(200,"application/json"),SUCCESS_JSON),
           "Get the queue data",
           &Api::GetQueue),
    ApiCall(HTTPClient::MGET,
           "^/performance/?$",
           create_map<response_t,string>(response_t(200,"application/json"),SUCCESS_JSON),
           "Get the performace data",
           &Api::GetPerformance),                                                                                                                                                                                                                                                                                 
    ApiCall(HTTPClient::MGET,
           "^/status/?$",
           create_map<response_t,string>(response_t(200,"application/json"),SUCCESS_JSON),
           "Get the status of the superfastmatch instance",
           &Api::GetStatus),
    ApiCall(HTTPClient::MGET,
          "^/histogram/?$",
          create_map<response_t,string>(response_t(200,"application/json"),SUCCESS_JSON),
          "Get a histogram of the index",
          &Api::GetHistogram),
    ApiCall(HTTPClient::MGET,
           "^/describe/?$",
           create_map<response_t,string>(response_t(200,"text/html"),DESCRIPTION_HTML),
           "Describe the superfastmatch API",
           &Api::GetDescription)                                                 
  };

  // ------------------------
  // Api call implementations
  // ------------------------

  void Api::DoSearch(const ApiParams& params,ApiResponse& response){
    map<string,string>::const_iterator text=params.form.find("text");
    if (text!=params.form.end()){
      SearchPtr search=Search::createTemporarySearch(registry_,params.body);
      search->fillJSONDictionary(&response.dict,false);
      response.type=response_t(200,"application/json");
    }else{
      response.type=response_t(500,"application/json");
      response.dict.SetValue("MESSAGE","No text field specified");  
    }
  }

  void Api::GetDocument(const ApiParams& params,ApiResponse& response){
    uint32_t doctype = kc::atoi(params.resource.find("doctype")->second.c_str());
    uint32_t docid = kc::atoi(params.resource.find("docid")->second.c_str());
    SearchPtr search=Search::getPermanentSearch(registry_,doctype,docid);
    if (search){
      search->fillJSONDictionary(&response.dict,true);
      response.type=response_t(200,"application/json");
    }else{
      response.type=response_t(404,"application/json");        
      response.dict.SetValue("MESSAGE","Document not found.");
    }
  }
  
  void Api::GetDocuments(const ApiParams& params,ApiResponse& response){
    string source;
    if (params.resource.find("source")!=params.resource.end()){
      source=params.resource.find("source")->second;
    }
    DocumentQuery query(registry_,source,"",params.query);
    if (query.isValid()){
      query.fillJSONDictionary(&response.dict);
      response.type=response_t(200,"application/json");
    }else{
      response.type=response_t(404,"application/json");        
      response.dict.SetValue("MESSAGE","Invalid query.");
    }
  }
  
  void Api::CreateDocument(const ApiParams& params,ApiResponse& response){
    uint32_t doctype = kc::atoi(params.resource.find("doctype")->second.c_str());
    uint32_t docid = kc::atoi(params.resource.find("docid")->second.c_str());
    map<string,string>::const_iterator text=params.form.find("text");
    if (text!=params.form.end()){
      CommandPtr addCommand = registry_->getQueueManager()->createCommand(AddDocument,doctype,docid,params.body);
      addCommand->fillDictionary(&response.dict);
      response.type=response_t(202,"application/json");        
    }else{
      response.type=response_t(500,"application/json");   
      response.dict.SetValue("MESSAGE","No text field specified");     
    }
  }

  void Api::CreateAndAssociateDocument(const ApiParams& params,ApiResponse& response){
    CreateDocument(params,response);
    if (response.type.first==202){
      AssociateDocument(params,response);
    }
  }
  
  void Api::DeleteDocument(const ApiParams& params,ApiResponse& response){
    uint32_t doctype = kc::atoi(params.resource.find("doctype")->second.c_str());
    uint32_t docid = kc::atoi(params.resource.find("docid")->second.c_str());
    CommandPtr deleteCommand = registry_->getQueueManager()->createCommand(DropDocument,doctype,docid,"");
    deleteCommand->fillDictionary(&response.dict);
    response.type=response_t(202,"application/json");        
  }
  
  void Api::AssociateDocument(const ApiParams& params,ApiResponse& response){
    uint32_t doctype = kc::atoi(params.resource.find("doctype")->second.c_str());
    uint32_t docid = kc::atoi(params.resource.find("docid")->second.c_str());
    CommandPtr associateCommand = registry_->getQueueManager()->createCommand(AddAssociation,doctype,docid,"");
    associateCommand->fillDictionary(&response.dict);
    response.type=response_t(202,"application/json");
  }
  
  void Api::AssociateDocuments(const ApiParams& params,ApiResponse& response){
    //TODO: Implement!
    CommandPtr associateCommand = registry_->getQueueManager()->createCommand(AddAssociations,0,0,"");
    associateCommand->fillDictionary(&response.dict);
    response.type=response_t(202,"application/json");
  }
  
  void Api::GetIndex(const ApiParams& params,ApiResponse& response){
    registry_->getPostings()->fillListDictionary(&response.dict,0);
    response.type=response_t(200,"application/json");
  }
  
  void Api::GetQueue(const ApiParams& params,ApiResponse& response){
    registry_->getQueueManager()->fillDictionary(&response.dict);
    response.type=response_t(200,"application/json");    
  }
  
  void Api::GetPerformance(const ApiParams& params,ApiResponse& response){
    registry_->fillPerformanceDictionary(&response.dict);
    response.type=response_t(200,"application/json");    
  }
  
  void Api::GetStatus(const ApiParams& params,ApiResponse& response){
    registry_->fillStatusDictionary(&response.dict);
    registry_->getPostings()->fillStatusDictionary(&response.dict);
    response.type=response_t(200,"application/json");
  }
  
  void Api::GetHistogram(const ApiParams& params,ApiResponse& response){
    registry_->getPostings()->fillHistogramDictionary(&response.dict);
    response.type=response_t(200,"application/json");
  }
  
  void Api::GetDescription(const ApiParams& params,ApiResponse& response){
    response.type=response_t(200,"application/json");
  }
}