#include <iostream>
#include <list>
#include <sqlite3.h>
#include "clang/Index/IndexRecordWriter.h"
#include "clang/Index/IndexRecordReader.h"
#include "clang/Index/IndexUnitReader.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

const std::string CREATE_SYMBOL_TABLE_STMNT = "create table symbol(id INTEGER primary key, usr varchar(50), name TEXT, kind TEXT, subkind TEXT, language TEXT)";
const std::string CREATE_OCCURRENCE_TABLE_STMNT = "create table occurrence(id INTEGER, symbol_id INTEGER, role TEXT, path TEXT, is_system BOOLEAN, line INTEGER, column INTEGER)";
const std::string CREATE_RELATION_TABLE_STMNT = "create table relation(occurrence_id INTEGER, symbol_id INTEGER, role TEXT)";

const std::string INSERT_SYMBOL_STMNT = "INSERT INTO symbol (id, usr, name, kind, subkind, language) VALUES (";
const std::string INSERT_OCCURRENCE_STMNT = "INSERT INTO occurrence (id, symbol_id, role, path, is_system, line, column) VALUES (";
const std::string INSERT_RELATION_STMNT = "INSERT INTO relation (occurrence_id, symbol_id, role) VALUES (";

class RecordSqliteDumper {
public:

    RecordSqliteDumper(const char *sqliteDBPath, StringRef dStorePath) {
        sqlite3_open(sqliteDBPath, &DB);
        char *errorMessage = 0;
        sqlite3_exec(DB, CREATE_SYMBOL_TABLE_STMNT.c_str(), NULL, 0, &errorMessage);
        sqlite3_exec(DB, CREATE_OCCURRENCE_TABLE_STMNT.c_str(), NULL, 0, &errorMessage);
        sqlite3_exec(DB, CREATE_RELATION_TABLE_STMNT.c_str(), NULL, 0, &errorMessage);
        dataStorePath = dStorePath;
        occurrenceIdCounter = 0;
    }
    
    ~RecordSqliteDumper() {
        sqlite3_close(DB);
    }
    
    int dumpToDatabase() {
        std::string error("");
        
        for (auto unitName: unitNames) {
            //auto indexReader = index::IndexUnitReader::createWithUnitFilename(StringRef("regexec.o-IYZYYYBU8KQO"), dataStorePath, error);
            auto indexReader = index::IndexUnitReader::createWithUnitFilename(StringRef(unitName), dataStorePath, error);
            auto depFn = [&](const index::IndexUnitReader::DependencyInfo &RO) -> bool {
                return dependencyInfoHandler(RO);
            };
            indexReader->foreachDependency(depFn);
        }
        return 0;
    }
    
private:
    sqlite3* DB;
    StringRef dataStorePath;
    u_int64_t occurrenceIdCounter;
    
    bool dependencyInfoHandler(const index::IndexUnitReader::DependencyInfo dependencyInfo) {
        if (dependencyInfo.Kind != index::IndexUnitReader::DependencyKind::Record) return true;
        
        std::string error("");
        auto recordReader = clang::index::IndexRecordReader::createWithRecordFilename(dependencyInfo.UnitOrRecordName, dataStorePath, error);
        
        auto declFn = [&](const clang::index::IndexRecordDecl *D) -> bool {
            return declHandler(D);
        };
        auto occurFn = [&](const index::IndexRecordOccurrence &RO) -> bool {
            return occurenceHandler(RO, dependencyInfo.FilePath, dependencyInfo.IsSystem);
        };
        recordReader->foreachDecl(true, declFn);
        recordReader->foreachOccurrence(occurFn);
        return true;
    }
    
    bool declHandler(const clang::index::IndexRecordDecl *decl) {
        auto declaration = (*decl);
        std::string query = (INSERT_SYMBOL_STMNT
                             + std::to_string(declaration.DeclID) + ", \""
                             + declaration.USR.str() + "\", \""
                             + declaration.Name.str() + "\", \""
                             + getKindName(declaration.SymInfo.Kind) + "\", \""
                             + getSubKindName(declaration.SymInfo.SubKind) + "\", \""
                             + getLangName(declaration.SymInfo.Lang) + "\")");
        char* messaggeError;
        sqlite3_exec(DB, query.c_str(), NULL, 0, &messaggeError);
        return true;
    }
    
    bool occurenceHandler(const clang::index::IndexRecordOccurrence &occurrence, StringRef filePath, bool isSystem) {
        for (auto role: getRoleNames(occurrence.Roles)) {
            std::string query = (INSERT_OCCURRENCE_STMNT
                                 + std::to_string(occurrenceIdCounter) + ", "
                                 + std::to_string(occurrence.Dcl->DeclID) + ", \""
                                 + role + "\", \""
                                 + filePath.str() + "\", "
                                 + std::to_string(isSystem) + ", "
                                 + std::to_string(occurrence.Line) + ", "
                                 + std::to_string(occurrence.Column) + ")");
            char* messaggeError;
            sqlite3_exec(DB, query.c_str(), NULL, 0, &messaggeError);
        }

        for (auto &rel: occurrence.Relations) {
            for (auto role: getRoleNames(rel.Roles)) {
                std::string query = (INSERT_RELATION_STMNT
                                     + std::to_string(occurrenceIdCounter) + ", "
                                     + std::to_string(rel.Dcl->DeclID) + ", \""
                                     + role + "\")");
                char* messaggeError;
                sqlite3_exec(DB, query.c_str(), NULL, 0, &messaggeError);
            }
        }
        
        occurrenceIdCounter++;
        return true;
    }

    std::string getKindName(index::SymbolKind kind) {
        switch (kind) {
            case index::SymbolKind::Unknown:
                return "Unknown";
            case index::SymbolKind::Module:
                return "Module";
            case index::SymbolKind::Namespace:
                return "Namespace";
            case index::SymbolKind::NamespaceAlias:
                return "NamespaceAlias";
            case index::SymbolKind::Macro:
                return "Macro";
            case index::SymbolKind::Enum:
                return "Enum";
            case index::SymbolKind::Struct:
                return "Struct";
            case index::SymbolKind::Class:
                return "Class";
            case index::SymbolKind::Protocol:
                return "Protocol";
            case index::SymbolKind::Extension:
                return "Extension";
            case index::SymbolKind::Union:
                return "Union";
            case index::SymbolKind::TypeAlias:
                return "TypeAlias";
            case index::SymbolKind::Function:
                return "Function";
            case index::SymbolKind::Variable:
                return "Variable";
            case index::SymbolKind::Field:
                return "Field";
            case index::SymbolKind::EnumConstant:
                return "EnumConstant";
            case index::SymbolKind::InstanceMethod:
                return "InstanceMethod";
            case index::SymbolKind::ClassMethod:
                return "ClassMethod";
            case index::SymbolKind::StaticMethod:
                return "StaticMethod";
            case index::SymbolKind::InstanceProperty:
                return "InstanceProperty";
            case clang::index::SymbolKind::ClassProperty:
                return "ClassProperty";
            case index::SymbolKind::StaticProperty:
                return "StaticProperty";
            case index::SymbolKind::Constructor:
                return "Constructor";
            case index::SymbolKind::Destructor:
                return "Destructor";
            case index::SymbolKind::ConversionFunction:
                return "ConversionFunction";
            case index::SymbolKind::Parameter:
                return "Parameter";
            case index::SymbolKind::Using:
                return "Using";
            case index::SymbolKind::CommentTag:
                return "CommentTag";
            case index::SymbolKind::TemplateTypeParm:
                return "TemplateTypeParm";
            case index::SymbolKind::TemplateTemplateParm:
                return "TemplateTemplateParm";
            case index::SymbolKind::NonTypeTemplateParm:
                return "NonTypeTemplateParm";
        }
        
    }
    
    std::string getSubKindName(index::SymbolSubKind subKind) {
        switch (subKind) {
            case index::SymbolSubKind::None:
                return "None";
            case index::SymbolSubKind::CXXCopyConstructor:
                return "CXXCopyConstructor";
            case index::SymbolSubKind::CXXMoveConstructor:
                return "CXXMoveConstructor";
            case index::SymbolSubKind::AccessorGetter:
                return "AccessorGetter";
            case index::SymbolSubKind::AccessorSetter:
                return "AccessorSetter";
            case index::SymbolSubKind::UsingTypename:
                return "UsingTypename";
            case index::SymbolSubKind::UsingValue:
                return "UsingValue";
            // Swift sub-kinds
            case index::SymbolSubKind::SwiftAccessorWillSet:
                return "SwiftAccessorWillSet";
            case index::SymbolSubKind::SwiftAccessorDidSet:
                return "SwiftAccessorDidSet";
            case index::SymbolSubKind::SwiftAccessorAddressor:
                return "SwiftAccessorAddressor";
            case index::SymbolSubKind::SwiftAccessorMutableAddressor:
                return "SwiftAccessorMutableAddressor";
            case index::SymbolSubKind::SwiftAccessorRead:
                return "SwiftAccessorRead";
            case index::SymbolSubKind::SwiftAccessorModify:
                return "SwiftAccessorModify";
            case index::SymbolSubKind::SwiftExtensionOfStruct:
                return "SwiftExtensionOfStruct";
            case index::SymbolSubKind::SwiftExtensionOfClass:
                return "SwiftExtensionOfClass";
            case index::SymbolSubKind::SwiftExtensionOfEnum:
                return "SwiftExtensionOfEnum";
            case index::SymbolSubKind::SwiftExtensionOfProtocol:
                return "SwiftExtensionOfProtocol";
            case index::SymbolSubKind::SwiftPrefixOperator:
                return "SwiftPrefixOperator";
            case index::SymbolSubKind::SwiftPostfixOperator:
                return "SwiftPostfixOperator";
            case index::SymbolSubKind::SwiftInfixOperator:
                return "SwiftInfixOperator";
            case index::SymbolSubKind::SwiftSubscript:
                return "SwiftSubscript";
            case index::SymbolSubKind::SwiftAssociatedType:
                return "SwiftAssociatedType";
            case index::SymbolSubKind::SwiftGenericTypeParam:
                return "SwiftGenericTypeParam";
        }
    }
    
    std::string getLangName(index::SymbolLanguage lang) {
        switch (lang) {
            case index::SymbolLanguage::ObjC:
                return "objc";
            case index::SymbolLanguage::C:
                return "c";
            case index::SymbolLanguage::CXX:
                return "cxx";
            case index::SymbolLanguage::Swift:
                return "swift";
        }
    }
    
    std::list<std::string> getRoleNames(index::SymbolRoleSet roles) {
        std::list<std::string> roleNames;

        if (roles & (index::SymbolRoleSet)index::SymbolRole::Declaration) {
            roleNames.push_back("Declaration");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::Definition) {
            roleNames.push_back("Definition");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::Reference) {
            roleNames.push_back("Reference");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::Read) {
            roleNames.push_back("Read");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::Write) {
            roleNames.push_back("Write");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::Call) {
            roleNames.push_back("Call");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::Dynamic) {
            roleNames.push_back("Dynamic");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::AddressOf) {
            roleNames.push_back("AddressOf");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::Implicit) {
            roleNames.push_back("Implicit");
        }
            // FIXME: this is not mirrored in CXSymbolRole.
            // Note that macro occurrences aren't currently supported in libclang.
        if (roles & (index::SymbolRoleSet)index::SymbolRole::Undefinition) { // macro #undef) {
            roleNames.push_back("Undefinition");
        }
            // Relation roles.
        if (roles & (index::SymbolRoleSet)index::SymbolRole::RelationChildOf) {
            roleNames.push_back("ChildOf");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::RelationBaseOf) {
            roleNames.push_back("BaseOf");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::RelationOverrideOf) {
            roleNames.push_back("OverrideOf");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::RelationReceivedBy) {
            roleNames.push_back("ReceivedBy");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::RelationCalledBy) {
            roleNames.push_back("CalledBy");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::RelationExtendedBy) {
            roleNames.push_back("ExtendedBy");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::RelationAccessorOf) {
            roleNames.push_back("AccessorOf");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::RelationContainedBy) {
            roleNames.push_back("ContainedBy");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::RelationIBTypeOf) {
            roleNames.push_back("IBTypeOf");
        }
        if (roles & (index::SymbolRoleSet)index::SymbolRole::RelationSpecializationOf) {
            roleNames.push_back("SpecializationOf");
        }
        return roleNames;
    }

    void print(std::list<std::string> const &list) {
        for (auto const& i: list) {
            std::cout << "\t\t\t" << i << "\n";
        }
    }

};


int main() {
    RecordSqliteDumper dumper("/Users/john/cpp.sqlite3", "/Users/john/Library/Developer/Xcode/DerivedData/LLVM-dyvuytlyfmtmcegqamkaftqcwljl/Index/DataStore");
    dumper.dumpToDatabase();
    std::cout << "DUMPING" << std::endl;


    return 0;
}
