#include "cqcode.h"

#include <regex>
#include <curl/curl.h>
#include <event2/event.h>

#include "md5.h"
#include "misc_functions.h"

using namespace std;

string message_escape(const string& msg)
{
    string tmp = msg;
    string_replace(tmp, "&", "&amp;");
    string_replace(tmp, "[", "&#91;");
    string_replace(tmp, "]", "&#93;");
    string_replace(tmp, ",", "&#44;");
    return tmp;
}

string message_unescape(const string& msg)
{
    string tmp = msg;
    string_replace(tmp, "&#91;", "[");
    string_replace(tmp, "&#93;", "]");
    string_replace(tmp, "&#44;", ",");
    string_replace(tmp, "&amp;", "&");
    return tmp;
}

//string enhance_cq_code_function_image(smatch& match);
//string enhance_cq_code_function_record(smatch& match);
string enhance_cq_code_remote_file(string data_dir, smatch& match);

string enhance_cq_code(const string& msg)
{
    string result;

    // 0: full CQ code function message, 1: function name, 2: params string
    regex exp("\\[CQ:([\\._\\-0-9A-Za-z]+?)(?:\\s*\\]|\\s*,\\s*(.*?)\\])");

    smatch match;
    string::const_iterator search_iter(msg.cbegin());
    while (regex_search(search_iter, msg.cend(), match, exp))
    {
        result += string(search_iter, search_iter + match.position()); // normal message before this current CQ code

        string function = match.str(1);
        if (function == "image")
            result += enhance_cq_code_remote_file("image", match);
        else if (function == "record")
            result += enhance_cq_code_remote_file("record", match);
        else
            result += match.str();

        search_iter += match.position() + match.length();
    }
    result += string(search_iter, msg.cend()); // add the rest plain text
    return result;
}

string enhance_cq_code_remote_file(string data_dir, smatch& match)
{
    // enhance CQ functions to support file from the internet or a different directory in filesystem
    string cqcode_call = match.str(0); // full CQ code function message
    string params = match.str(2);
    smatch m;
    if (regex_search(params, m, regex("file=(https?:\\/\\/[^,\\?]+(\\.[^\\s,\\?]+)\\??[^,\\?]*)")))
    {
        string raw_url = m.str(1);
        string url = message_unescape(raw_url);
        string ext = m.str(2);
        MD5 md5(url);
        string hash = md5.toStr();
        string filename = hash + ext;

        string filepath = get_cq_root_path() + "data\\" + data_dir + "\\" + filename;
        FILE* fp = NULL;
        fopen_s(&fp, filepath.c_str(), "wb");
        if (fp)
        {
            CURL* curl = curl_easy_init();
            curl_easy_setopt(curl, CURLOPT_URL, url);

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_file_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

            struct curl_slist* chunk = NULL;
            chunk = curl_slist_append(chunk,
                                      "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                      "AppleWebKit/537.36 (KHTML, like Gecko) "
                                      "Chrome/56.0.2924.87 Safari/537.36");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

            curl_easy_perform(curl); // download remote file

            fclose(fp);
            curl_easy_cleanup(curl);
            curl_slist_free_all(chunk);
        }
        string_replace(cqcode_call, raw_url, filename);
    }
    else if (regex_search(params, m, regex("file=file:\\/\\/([^,\\?]+(\\.[^\\s,\\?]+))")))
    {
        string raw_path = m.str(1);
        string path = message_unescape(raw_path);
        string ext = m.str(2);
        MD5 md5(path);
        string hash = md5.toStr();
        string new_filename = hash + ext;

        string new_filepath = get_cq_root_path() + "data\\" + data_dir + "\\" + new_filename;
        wstring path_wstr = utf8_to_wstr(path.c_str());
        wstring new_filepath_wstr = utf8_to_wstr(new_filepath.c_str());
        CopyFileW(path_wstr.c_str(), new_filepath_wstr.c_str(), false); // copy remote file

        string_replace(cqcode_call, "file://" + raw_path, new_filename);
    }
    return cqcode_call;
}
