/*
 * This file is released under the terms of the Artistic License.  Please see
 * the file LICENSE, included in this package, for details.
 *
 * Copyright The DBT-5 Authors
 */

#include <catalog/pg_type_d.h>

#include "DBConnectionServerSide.h"

// These are inlined function that should only be used here.

bool inline check_count(int should, int is, const char *file, int line)
{
	if (should != is) {
		cout << "*** array length (" << is << ") does not match expections ("
			 << should << "): " << file << ":" << line << endl;
		return false;
	}
	return true;
}

int inline get_col_num(PGresult *res, const char *col_name)
{
	int col_num = PQfnumber(res, col_name);
	if (col_num == -1) {
		cerr << "ERROR: column " << col_name << " not found" << endl;
		exit(1);
	}
	return col_num;
}

void inline TokenizeArray2(const string &str2, vector<string> &tokens)
{
	// This is essentially an empty array. i.e. '()'
	if (str2.size() < 3)
		return;

	// We only call this function because we need to chop up arrays that
	// are in the format '{(1,2,3),(a,b,c)}', so trim off the braces.
	string str = str2.substr(1, str2.size() - 2);

	// Skip delimiters at beginning.
	string::size_type lastPos = str.find_first_of("(", 0);
	// Find first "non-delimiter".
	string::size_type pos = str.find_first_of(")", lastPos);

	while (string::npos != pos || string::npos != lastPos) {
		// Found a token, add it to the vector.
		tokens.push_back(str.substr(lastPos, pos - lastPos + 1));

		lastPos = str.find_first_of("(", pos);
		pos = str.find_first_of(")", lastPos);
	}
}

// String Tokenizer
// FIXME: This token doesn't handle strings with escaped characters.
void inline TokenizeSmart(const string &str, vector<string> &tokens)
{
	// This is essentially an empty array. i.e. '{}'
	if (str.size() < 3)
		return;

	string::size_type lastPos = 1;
	string::size_type pos = 1;
	bool end = false;
	while (end == false) {
		if (str[lastPos] == '"') {
			pos = str.find_first_of("\"", lastPos + 1);
			if (pos == string::npos) {
				pos = str.find_first_of("}", lastPos);
				end = true;
			}
			tokens.push_back(str.substr(lastPos + 1, pos - lastPos - 1));
			lastPos = pos + 2;
		} else if (str[lastPos] == '\0') {
			return;
		} else {
			pos = str.find_first_of(",", lastPos);
			if (pos == string::npos) {
				pos = str.find_first_of("}", lastPos);
				end = true;
			}
			tokens.push_back(str.substr(lastPos, pos - lastPos));
			lastPos = pos + 1;
		}
	}
}

CDBConnectionServerSide::CDBConnectionServerSide(const char *szHost,
		const char *szDBName, const char *szDBPort, bool bVerbose)
: CDBConnection(szHost, szDBName, szDBPort, bVerbose)
{
}

CDBConnectionServerSide::~CDBConnectionServerSide() {}

void
CDBConnectionServerSide::execute(
		const TBrokerVolumeFrame1Input *pIn, TBrokerVolumeFrame1Output *pOut)
{
	ostringstream osBrokers;
	int i = 0;
	osBrokers << pIn->broker_list[i];
	for (i = 1; pIn->broker_list[i][0] != '\0' && i < max_broker_list_len;
			i++) {
		osBrokers << ", " << pIn->broker_list[i];
	}

	ostringstream osSQL;
	osSQL << "SELECT * FROM BrokerVolumeFrame1('{" << osBrokers.str() << "}','"
		  << pIn->sector_name << "')";

	PGresult *res = exec(osSQL.str().c_str());
	int i_broker_name = get_col_num(res, "broker_name");
	int i_list_len = get_col_num(res, "list_len");
	int i_volume = get_col_num(res, "volume");

	pOut->list_len = atoi(PQgetvalue(res, 0, i_list_len));

	vector<string> vAux;

	TokenizeSmart(PQgetvalue(res, 0, i_broker_name), vAux);
	for (size_t j = 0; j < vAux.size(); ++j) {
		strncpy(pOut->broker_name[j], vAux[j].c_str(), cB_NAME_len);
		pOut->broker_name[j][cB_NAME_len] = '\0';
	}
	check_count(pOut->list_len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_volume), vAux);
	for (size_t j = 0; j < vAux.size(); ++j) {
		pOut->volume[j] = atof(vAux[j].c_str());
	}
	check_count(pOut->list_len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();
	PQclear(res);
}

void
CDBConnectionServerSide::execute(const TCustomerPositionFrame1Input *pIn,
		TCustomerPositionFrame1Output *pOut)
{
	uint64_t cust_id = htobe64((uint64_t) pIn->cust_id);

	const char *paramValues[2] = { (char *) &cust_id, pIn->tax_id };
	const int paramLengths[2]
			= { sizeof(uint64_t), sizeof(char) * (cTAX_ID_len + 1) };
	const int paramFormats[2] = { 1, 0 };

	PGresult *res = exec("SELECT * FROM CustomerPositionFrame1($1, $2)", 2,
			NULL, paramValues, paramLengths, paramFormats, 0);

	int i_cust_id = get_col_num(res, "cust_id");
	int i_acct_id = get_col_num(res, "acct_id");
	int i_acct_len = get_col_num(res, "acct_len");
	int i_asset_total = get_col_num(res, "asset_total");
	int i_c_ad_id = get_col_num(res, "c_ad_id");
	int i_c_area_1 = get_col_num(res, "c_area_1");
	int i_c_area_2 = get_col_num(res, "c_area_2");
	int i_c_area_3 = get_col_num(res, "c_area_3");
	int i_c_ctry_1 = get_col_num(res, "c_ctry_1");
	int i_c_ctry_2 = get_col_num(res, "c_ctry_2");
	int i_c_ctry_3 = get_col_num(res, "c_ctry_3");
	int i_c_dob = get_col_num(res, "c_dob");
	int i_c_email_1 = get_col_num(res, "c_email_1");
	int i_c_email_2 = get_col_num(res, "c_email_2");
	int i_c_ext_1 = get_col_num(res, "c_ext_1");
	int i_c_ext_2 = get_col_num(res, "c_ext_2");
	int i_c_ext_3 = get_col_num(res, "c_ext_3");
	int i_c_f_name = get_col_num(res, "c_f_name");
	int i_c_gndr = get_col_num(res, "c_gndr");
	int i_c_l_name = get_col_num(res, "c_l_name");
	int i_c_local_1 = get_col_num(res, "c_local_1");
	int i_c_local_2 = get_col_num(res, "c_local_2");
	int i_c_local_3 = get_col_num(res, "c_local_3");
	int i_c_m_name = get_col_num(res, "c_m_name");
	int i_c_st_id = get_col_num(res, "c_st_id");
	int i_c_tier = get_col_num(res, "c_tier");
	int i_cash_bal = get_col_num(res, "cash_bal");

	pOut->acct_len = atoi(PQgetvalue(res, 0, i_acct_len));
	pOut->cust_id = atoll(PQgetvalue(res, 0, i_cust_id));

	vector<string> vAux;

	TokenizeSmart(PQgetvalue(res, 0, i_acct_id), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->acct_id[i] = atoll(vAux[i].c_str());
	}
	check_count(pOut->acct_len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_asset_total), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->asset_total[i] = atof(vAux[i].c_str());
	}
	check_count(pOut->acct_len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	pOut->c_ad_id = atoll(PQgetvalue(res, 0, i_c_ad_id));

	strncpy(pOut->c_area_1, PQgetvalue(res, 0, i_c_area_1), cAREA_len);
	pOut->c_area_1[cAREA_len] = '\0';
	strncpy(pOut->c_area_2, PQgetvalue(res, 0, i_c_area_2), cAREA_len);
	pOut->c_area_2[cAREA_len] = '\0';
	strncpy(pOut->c_area_3, PQgetvalue(res, 0, i_c_area_3), cAREA_len);
	pOut->c_area_3[cAREA_len] = '\0';

	strncpy(pOut->c_ctry_1, PQgetvalue(res, 0, i_c_ctry_1), cCTRY_len);
	pOut->c_ctry_1[cCTRY_len] = '\0';
	strncpy(pOut->c_ctry_2, PQgetvalue(res, 0, i_c_ctry_2), cCTRY_len);
	pOut->c_ctry_2[cCTRY_len] = '\0';
	strncpy(pOut->c_ctry_3, PQgetvalue(res, 0, i_c_ctry_3), cCTRY_len);
	pOut->c_ctry_3[cCTRY_len] = '\0';

	sscanf(PQgetvalue(res, 0, i_c_dob), "%hd-%hd-%hd", &pOut->c_dob.year,
			&pOut->c_dob.month, &pOut->c_dob.day);

	strncpy(pOut->c_email_1, PQgetvalue(res, 0, i_c_email_1), cEMAIL_len);
	pOut->c_email_1[cEMAIL_len] = '\0';
	strncpy(pOut->c_email_2, PQgetvalue(res, 0, i_c_email_2), cEMAIL_len);
	pOut->c_email_2[cEMAIL_len] = '\0';

	strncpy(pOut->c_ext_1, PQgetvalue(res, 0, i_c_ext_1), cEXT_len);
	pOut->c_ext_1[cEXT_len] = '\0';
	strncpy(pOut->c_ext_2, PQgetvalue(res, 0, i_c_ext_2), cEXT_len);
	pOut->c_ext_2[cEXT_len] = '\0';
	strncpy(pOut->c_ext_3, PQgetvalue(res, 0, i_c_ext_3), cEXT_len);
	pOut->c_ext_3[cEXT_len] = '\0';

	strncpy(pOut->c_f_name, PQgetvalue(res, 0, i_c_f_name), cF_NAME_len);
	pOut->c_f_name[cF_NAME_len] = '\0';
	strncpy(pOut->c_gndr, PQgetvalue(res, 0, i_c_gndr), cGNDR_len);
	pOut->c_gndr[cGNDR_len] = '\0';
	strncpy(pOut->c_l_name, PQgetvalue(res, 0, i_c_l_name), cL_NAME_len);
	pOut->c_l_name[cL_NAME_len] = '\0';

	strncpy(pOut->c_local_1, PQgetvalue(res, 0, i_c_local_1), cLOCAL_len);
	pOut->c_local_1[cLOCAL_len] = '\0';
	strncpy(pOut->c_local_2, PQgetvalue(res, 0, i_c_local_2), cLOCAL_len);
	pOut->c_local_2[cLOCAL_len] = '\0';
	strncpy(pOut->c_local_3, PQgetvalue(res, 0, i_c_local_3), cLOCAL_len);
	pOut->c_local_3[cLOCAL_len] = '\0';

	strncpy(pOut->c_m_name, PQgetvalue(res, 0, i_c_m_name), cM_NAME_len);
	pOut->c_m_name[cM_NAME_len] = '\0';
	strncpy(pOut->c_st_id, PQgetvalue(res, 0, i_c_st_id), cST_ID_len);
	pOut->c_st_id[cST_ID_len] = '\0';
	strncpy(&pOut->c_tier, PQgetvalue(res, 0, i_c_tier), 1);

	TokenizeSmart(PQgetvalue(res, 0, i_cash_bal), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->cash_bal[i] = atof(vAux[i].c_str());
	}
	check_count(pOut->acct_len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();
	PQclear(res);
}

void
CDBConnectionServerSide::execute(const TCustomerPositionFrame2Input *pIn,
		TCustomerPositionFrame2Output *pOut)
{
	uint64_t acct_id = htobe64((uint64_t) pIn->acct_id);

	const char *paramValues[1] = { (char *) &acct_id };
	const int paramLengths[1] = { sizeof(uint64_t) };
	const int paramFormats[1] = { 1 };

	PGresult *res = exec("SELECT * FROM CustomerPositionFrame2($1)", 1, NULL,
			paramValues, paramLengths, paramFormats, 0);

	int i_hist_dts = get_col_num(res, "hist_dts");
	int i_hist_len = get_col_num(res, "hist_len");
	int i_qty = get_col_num(res, "qty");
	int i_symbol = get_col_num(res, "symbol");
	int i_trade_id = get_col_num(res, "trade_id");
	int i_trade_status = get_col_num(res, "trade_status");

	pOut->hist_len = atoi(PQgetvalue(res, 0, i_hist_len));

	vector<string> vAux;

	TokenizeSmart(PQgetvalue(res, 0, i_hist_dts), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->hist_dts[i].year, &pOut->hist_dts[i].month,
				&pOut->hist_dts[i].day, &pOut->hist_dts[i].hour,
				&pOut->hist_dts[i].minute, &pOut->hist_dts[i].second);
	}
	check_count(pOut->hist_len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_qty), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->qty[i] = atoi(vAux[i].c_str());
	}
	check_count(pOut->hist_len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_symbol), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->symbol[i], vAux[i].c_str(), cSYMBOL_len);
		pOut->symbol[i][cSYMBOL_len] = '\0';
	}
	check_count(pOut->hist_len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_id), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_id[i] = atoll(vAux[i].c_str());
	}
	check_count(pOut->hist_len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_status), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_status[i], vAux[i].c_str(), cST_NAME_len);
		pOut->trade_status[i][cST_NAME_len] = '\0';
	}
	check_count(pOut->hist_len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();
	PQclear(res);
}

void
CDBConnectionServerSide::execute(const TDataMaintenanceFrame1Input *pIn)
{
	uint64_t acct_id = htobe64((uint64_t) pIn->acct_id);
	uint64_t c_id = htobe64((uint64_t) pIn->c_id);
	uint64_t co_id = htobe64((uint64_t) pIn->co_id);
	uint32_t day_of_month = htobe32((uint32_t) pIn->day_of_month);
	uint32_t vol_incr = htobe32((uint32_t) pIn->vol_incr);

	const char *paramValues[8] = { (char *) &acct_id, (char *) &c_id,
		(char *) &co_id, (char *) &day_of_month, pIn->symbol, pIn->table_name,
		pIn->tx_id, (char *) &vol_incr };
	const int paramLengths[8] = { sizeof(uint64_t), sizeof(uint64_t),
		sizeof(uint64_t), sizeof(uint32_t), sizeof(char) * (cSYMBOL_len + 1),
		sizeof(char) * (max_table_name + 1), sizeof(char) * (cTAX_ID_len + 1),
		sizeof(uint32_t) };
	const int paramFormats[8] = { 1, 1, 1, 1, 0, 0, 0, 1 };

	PGresult *res = exec("SELECT * FROM DataMaintenanceFrame1($1, $2, $3, $4, "
						 "$5, $6, $7, $8)",
			8, NULL, paramValues, paramLengths, paramFormats, 0);
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TMarketWatchFrame1Input *pIn, TMarketWatchFrame1Output *pOut)
{
	uint64_t acct_id = htobe64((uint64_t) pIn->acct_id);
	uint64_t c_id = htobe64((uint64_t) pIn->c_id);
	uint64_t ending_co_id = htobe64((uint64_t) pIn->ending_co_id);
	struct tm tm = { 0 };
	tm.tm_year = pIn->start_day.year - 1900;
	tm.tm_mon = pIn->start_day.month - 1;
	tm.tm_mday = pIn->start_day.day;
	mktime(&tm);
	uint32_t start_day
			= htobe32((uint32_t) ((tm.tm_year - 100) * 365
								  + (tm.tm_year - 100) / 4 + tm.tm_yday));
	uint64_t starting_co_id = htobe64((uint64_t) pIn->starting_co_id);

	const Oid paramTypes[6]
			= { INT8OID, INT8OID, INT8OID, TEXTOID, DATEOID, INT8OID };
	const char *paramValues[6] = { (char *) &acct_id, (char *) &c_id,
		(char *) &ending_co_id, pIn->industry_name, (char *) &start_day,
		(char *) &starting_co_id };
	const int paramLengths[6] = { sizeof(uint64_t), sizeof(uint64_t),
		sizeof(uint64_t), sizeof(char) * (cIN_NAME_len + 1), sizeof(uint32_t),
		sizeof(uint64_t) };
	const int paramFormats[6] = { 1, 1, 1, 0, 1, 1 };

	stringstream ss;
	ss << pIn->start_day.year << "-" 
	   << pIn->start_day.month << "-" 
	   << pIn->start_day.day;
	replace_map[4] = ss.str();

	PGresult *res
			= exec("SELECT * FROM MarketWatchFrame1($1, $2, $3, $4, $5, $6)",
					6, paramTypes, paramValues, paramLengths, paramFormats, 0);

	pOut->pct_change = atof(PQgetvalue(res, 0, 0));
	PQclear(res);
}

void
CDBConnectionServerSide::execute(const TSecurityDetailFrame1Input *pIn,
		TSecurityDetailFrame1Output *pOut)
{
	uint16_t access_lob_flag = htobe16(pIn->access_lob_flag ? 1 : 0);
	uint32_t max_rows_to_return = htobe32((uint32_t) pIn->max_rows_to_return);
	struct tm tm = { 0 };
	tm.tm_year = pIn->start_day.year - 1900;
	tm.tm_mon = pIn->start_day.month - 1;
	tm.tm_mday = pIn->start_day.day;
	mktime(&tm);
	uint32_t start_day
			= htobe32((uint32_t) ((tm.tm_year - 100) * 365
								  + (tm.tm_year - 100) / 4 + tm.tm_yday));

	const Oid paramTypes[4]
			= { INT2OID, INT4OID, DATEOID, TEXTOID };
	const char *paramValues[4] = { (char *) &access_lob_flag,
		(char *) &max_rows_to_return, (char *) &start_day, pIn->symbol };
	const int paramLengths[4] = { sizeof(uint16_t), sizeof(uint32_t),
		sizeof(uint32_t), sizeof(char) * (cSYMBOL_len + 1) };
	const int paramFormats[4] = { 1, 1, 1, 0 };

	stringstream ss;
	ss << pIn->start_day.year << "-" 
	   << pIn->start_day.month << "-" 
	   << pIn->start_day.day;
	replace_map[2] = ss.str();

	PGresult *res = exec("SELECT * FROM SecurityDetailFrame1($1, $2, $3, $4)",
			4, paramTypes, paramValues, paramLengths, paramFormats, 0);

	int i_s52_wk_high = get_col_num(res, "x52_wk_high");
	int i_s52_wk_high_date = get_col_num(res, "x52_wk_high_date");
	int i_s52_wk_low = get_col_num(res, "x52_wk_low");
	int i_s52_wk_low_date = get_col_num(res, "x52_wk_low_date");
	int i_ceo_name = get_col_num(res, "ceo_name");
	int i_co_ad_cty = get_col_num(res, "co_ad_ctry");
	int i_co_ad_div = get_col_num(res, "co_ad_div");
	int i_co_ad_line1 = get_col_num(res, "co_ad_line1");
	int i_co_ad_line2 = get_col_num(res, "co_ad_line2");
	int i_co_ad_town = get_col_num(res, "co_ad_town");
	int i_co_ad_zip = get_col_num(res, "co_ad_zip");
	int i_co_desc = get_col_num(res, "co_desc");
	int i_co_name = get_col_num(res, "co_name");
	int i_co_st_id = get_col_num(res, "co_st_id");
	int i_cp_co_name = get_col_num(res, "cp_co_name");
	int i_cp_in_name = get_col_num(res, "cp_in_name");
	int i_day = get_col_num(res, "day");
	int i_day_len = get_col_num(res, "day_len");
	int i_divid = get_col_num(res, "divid");
	int i_ex_ad_cty = get_col_num(res, "ex_ad_ctry");
	int i_ex_ad_div = get_col_num(res, "ex_ad_div");
	int i_ex_ad_line1 = get_col_num(res, "ex_ad_line1");
	int i_ex_ad_line2 = get_col_num(res, "ex_ad_line2");
	int i_ex_ad_town = get_col_num(res, "ex_ad_town");
	int i_ex_ad_zip = get_col_num(res, "ex_ad_zip");
	int i_ex_close = get_col_num(res, "ex_close");
	int i_ex_date = get_col_num(res, "ex_date");
	int i_ex_desc = get_col_num(res, "ex_desc");
	int i_ex_name = get_col_num(res, "ex_name");
	int i_ex_num_symb = get_col_num(res, "ex_num_symb");
	int i_ex_open = get_col_num(res, "ex_open");
	int i_fin = get_col_num(res, "fin");
	int i_fin_len = get_col_num(res, "fin_len");
	int i_last_open = get_col_num(res, "last_open");
	int i_last_price = get_col_num(res, "last_price");
	int i_last_vol = get_col_num(res, "last_vol");
	int i_news = get_col_num(res, "news");
	int i_news_len = get_col_num(res, "news_len");
	int i_num_out = get_col_num(res, "num_out");
	int i_open_date = get_col_num(res, "open_date");
	int i_pe_ratio = get_col_num(res, "pe_ratio");
	int i_s_name = get_col_num(res, "s_name");
	int i_sp_rate = get_col_num(res, "sp_rate");
	int i_start_date = get_col_num(res, "start_date");
	int i_yield = get_col_num(res, "yield");

	pOut->fin_len = atoi(PQgetvalue(res, 0, i_fin_len));
	pOut->day_len = atoi(PQgetvalue(res, 0, i_day_len));
	pOut->news_len = atoi(PQgetvalue(res, 0, i_news_len));

	pOut->s52_wk_high = atof(PQgetvalue(res, 0, i_s52_wk_high));
	sscanf(PQgetvalue(res, 0, i_s52_wk_high_date), "%hd-%hd-%hd",
			&pOut->s52_wk_high_date.year, &pOut->s52_wk_high_date.month,
			&pOut->s52_wk_high_date.day);
	pOut->s52_wk_low = atof(PQgetvalue(res, 0, i_s52_wk_low));
	sscanf(PQgetvalue(res, 0, i_s52_wk_low_date), "%hd-%hd-%hd",
			&pOut->s52_wk_low_date.year, &pOut->s52_wk_low_date.month,
			&pOut->s52_wk_low_date.day);

	strncpy(pOut->ceo_name, PQgetvalue(res, 0, i_ceo_name), cCEO_NAME_len);
	pOut->ceo_name[cCEO_NAME_len] = '\0';
	strncpy(pOut->co_ad_cty, PQgetvalue(res, 0, i_co_ad_cty), cAD_CTRY_len);
	pOut->co_ad_cty[cAD_CTRY_len] = '\0';
	strncpy(pOut->co_ad_div, PQgetvalue(res, 0, i_co_ad_div), cAD_DIV_len);
	pOut->co_ad_div[cAD_DIV_len] = '\0';
	strncpy(pOut->co_ad_line1, PQgetvalue(res, 0, i_co_ad_line1),
			cAD_LINE_len);
	pOut->co_ad_line1[cAD_LINE_len] = '\0';
	strncpy(pOut->co_ad_line2, PQgetvalue(res, 0, i_co_ad_line2),
			cAD_LINE_len);
	pOut->co_ad_line2[cAD_LINE_len] = '\0';
	strncpy(pOut->co_ad_town, PQgetvalue(res, 0, i_co_ad_town), cAD_TOWN_len);
	pOut->co_ad_town[cAD_TOWN_len] = '\0';
	strncpy(pOut->co_ad_zip, PQgetvalue(res, 0, i_co_ad_zip), cAD_ZIP_len);
	pOut->co_ad_zip[cAD_ZIP_len] = '\0';
	strncpy(pOut->co_desc, PQgetvalue(res, 0, i_co_desc), cCO_DESC_len);
	pOut->co_desc[cCO_DESC_len] = '\0';
	strncpy(pOut->co_name, PQgetvalue(res, 0, i_co_name), cCO_NAME_len);
	pOut->co_name[cCO_NAME_len] = '\0';
	strncpy(pOut->co_st_id, PQgetvalue(res, 0, i_co_st_id), cST_ID_len);
	pOut->co_st_id[cST_ID_len] = '\0';

	vector<string> vAux;

	TokenizeSmart(PQgetvalue(res, 0, i_cp_co_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->cp_co_name[i], vAux[i].c_str(), cCO_NAME_len);
		pOut->cp_co_name[i][cCO_NAME_len] = '\0';
	}
	// FIXME: The stored functions for PostgreSQL are designed to return 3
	// items in the array, even though it's not required.
	check_count(3, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cp_in_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->cp_in_name[i], vAux[i].c_str(), cIN_NAME_len);
		pOut->cp_in_name[i][cIN_NAME_len] = '\0';
	}

	// FIXME: The stored functions for PostgreSQL are designed to return 3
	// items in the array, even though it's not required.
	check_count(3, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeArray2(PQgetvalue(res, 0, i_day), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		vector<string> v2;
		vector<string>::iterator p2;

		TokenizeSmart(vAux[i].c_str(), v2);

		p2 = v2.begin();
		sscanf((*p2++).c_str(), "%hd-%hd-%hd", &pOut->day[i].date.year,
				&pOut->day[i].date.month, &pOut->day[i].date.day);
		pOut->day[i].close = atof((*p2++).c_str());
		pOut->day[i].high = atof((*p2++).c_str());
		pOut->day[i].low = atof((*p2++).c_str());
		pOut->day[i].vol = atoi((*p2++).c_str());
		v2.clear();
	}
	check_count(pOut->day_len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	pOut->divid = atof(PQgetvalue(res, 0, i_divid));

	strncpy(pOut->ex_ad_cty, PQgetvalue(res, 0, i_ex_ad_cty), cAD_CTRY_len);
	pOut->ex_ad_cty[cAD_CTRY_len] = '\0';
	strncpy(pOut->ex_ad_div, PQgetvalue(res, 0, i_ex_ad_div), cAD_DIV_len);
	pOut->ex_ad_div[cAD_DIV_len] = '\0';
	strncpy(pOut->ex_ad_line1, PQgetvalue(res, 0, i_ex_ad_line1),
			cAD_LINE_len);
	pOut->ex_ad_line1[cAD_LINE_len] = '\0';
	strncpy(pOut->ex_ad_line2, PQgetvalue(res, 0, i_ex_ad_line2),
			cAD_LINE_len);
	pOut->ex_ad_line2[cAD_LINE_len] = '\0';
	strncpy(pOut->ex_ad_town, PQgetvalue(res, 0, i_ex_ad_town), cAD_TOWN_len);
	pOut->ex_ad_town[cAD_TOWN_len] = '\0';
	strncpy(pOut->ex_ad_zip, PQgetvalue(res, 0, i_ex_ad_zip), cAD_ZIP_len);
	pOut->ex_ad_zip[cAD_ZIP_len] = '\0';
	pOut->ex_close = atoi(PQgetvalue(res, 0, i_ex_close));
	sscanf(PQgetvalue(res, 0, i_ex_date), "%hd-%hd-%hd", &pOut->ex_date.year,
			&pOut->ex_date.month, &pOut->ex_date.day);
	strncpy(pOut->ex_desc, PQgetvalue(res, 0, i_ex_desc), cEX_DESC_len);
	pOut->ex_desc[cEX_DESC_len] = '\0';
	strncpy(pOut->ex_name, PQgetvalue(res, 0, i_ex_name), cEX_NAME_len);
	pOut->ex_name[cEX_NAME_len] = '\0';
	pOut->ex_num_symb = atoi(PQgetvalue(res, 0, i_ex_num_symb));
	pOut->ex_open = atoi(PQgetvalue(res, 0, i_ex_open));

	TokenizeArray2(PQgetvalue(res, 0, i_fin), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		vector<string> v2;
		vector<string>::iterator p2;

		TokenizeSmart(vAux[i].c_str(), v2);

		p2 = v2.begin();
		pOut->fin[i].year = atoi((*p2++).c_str());
		pOut->fin[i].qtr = atoi((*p2++).c_str());
		sscanf((*p2++).c_str(), "%hd-%hd-%hd", &pOut->fin[i].start_date.year,
				&pOut->fin[i].start_date.month, &pOut->fin[i].start_date.day);
		pOut->fin[i].rev = atof((*p2++).c_str());
		pOut->fin[i].net_earn = atof((*p2++).c_str());
		pOut->fin[i].basic_eps = atof((*p2++).c_str());
		pOut->fin[i].dilut_eps = atof((*p2++).c_str());
		pOut->fin[i].margin = atof((*p2++).c_str());
		pOut->fin[i].invent = atof((*p2++).c_str());
		pOut->fin[i].assets = atof((*p2++).c_str());
		pOut->fin[i].liab = atof((*p2++).c_str());
		pOut->fin[i].out_basic = atof((*p2++).c_str());
		pOut->fin[i].out_dilut = atof((*p2++).c_str());
		v2.clear();
	}
	check_count(pOut->fin_len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	pOut->last_open = atof(PQgetvalue(res, 0, i_last_open));
	pOut->last_price = atof(PQgetvalue(res, 0, i_last_price));
	pOut->last_vol = atoi(PQgetvalue(res, 0, i_last_vol));

	TokenizeArray2(PQgetvalue(res, 0, i_news), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		vector<string> v2;
		vector<string>::iterator p2;

		TokenizeSmart(vAux[i].c_str(), v2);

		p2 = v2.begin();
		// FIXME: Postgresql can actually return 5 times the amount of data due
		// to escaped characters.  Cap the data at the length that EGen defines
		// it and hope it isn't a problem for continuing the test correctly.
		strncpy(pOut->news[i].item, (*p2++).c_str(), cNI_ITEM_len);
		pOut->news[i].item[cNI_ITEM_len] = '\0';
		sscanf((*p2++).c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->news[i].dts.year, &pOut->news[i].dts.month,
				&pOut->news[i].dts.day, &pOut->news[i].dts.hour,
				&pOut->news[i].dts.minute, &pOut->news[i].dts.second);
		strncpy(pOut->news[i].src, (*p2++).c_str(), cNI_SOURCE_len);
		pOut->news[i].src[cNI_SOURCE_len] = '\0';
		strncpy(pOut->news[i].auth, (*p2++).c_str(), cNI_AUTHOR_len);
		pOut->news[i].auth[cNI_AUTHOR_len] = '\0';
		strncpy(pOut->news[i].headline, (*p2++).c_str(), cNI_HEADLINE_len);
		pOut->news[i].headline[cNI_HEADLINE_len] = '\0';
		strncpy(pOut->news[i].summary, (*p2++).c_str(), cNI_SUMMARY_len);
		pOut->news[i].summary[cNI_SUMMARY_len] = '\0';
		v2.clear();
	}
	check_count(pOut->news_len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	sscanf(PQgetvalue(res, 0, i_open_date), "%hd-%hd-%hd",
			&pOut->open_date.year, &pOut->open_date.month,
			&pOut->open_date.day);
	pOut->pe_ratio = atof(PQgetvalue(res, 0, i_pe_ratio));
	strncpy(pOut->s_name, PQgetvalue(res, 0, i_s_name), cS_NAME_len);
	pOut->s_name[cS_NAME_len] = '\0';
	pOut->num_out = atoll(PQgetvalue(res, 0, i_num_out));
	strncpy(pOut->sp_rate, PQgetvalue(res, 0, i_sp_rate), cSP_RATE_len);
	pOut->sp_rate[cSP_RATE_len] = '\0';
	sscanf(PQgetvalue(res, 0, i_start_date), "%hd-%hd-%hd",
			&pOut->start_date.year, &pOut->start_date.month,
			&pOut->start_date.day);
	pOut->yield = atof(PQgetvalue(res, 0, i_yield));
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeLookupFrame1Input *pIn, TTradeLookupFrame1Output *pOut)
{
	ostringstream osTrades;
	int j = 0;
	osTrades << "{" << pIn->trade_id[j];
	for (j = 1; j < pIn->max_trades; j++) {
		osTrades << "," << pIn->trade_id[j];
	}
	osTrades << "}";

	char trade_id[osTrades.str().length() + 1];
	strncpy(trade_id, osTrades.str().c_str(), osTrades.str().length());
	trade_id[osTrades.str().length()] = '\0';

	uint32_t max_trades = htobe32((uint32_t) pIn->max_trades);

	const char *paramValues[2] = { (char *) &max_trades, trade_id };
	const int paramLengths[2] = { sizeof(uint32_t),
		(int) sizeof(char) * ((int) osTrades.str().length() + 1) };
	const int paramFormats[2] = { 1, 0 };

	PGresult *res = exec("SELECT * FROM TradeLookupFrame1($1, $2)", 2, NULL,
			paramValues, paramLengths, paramFormats, 0);

	int i_bid_price = get_col_num(res, "bid_price");
	int i_cash_transaction_amount
			= get_col_num(res, "cash_transaction_amount");
	int i_cash_transaction_dts = get_col_num(res, "cash_transaction_dts");
	int i_cash_transaction_name = get_col_num(res, "cash_transaction_name");
	int i_exec_name = get_col_num(res, "exec_name");
	int i_is_cash = get_col_num(res, "is_cash");
	int i_is_market = get_col_num(res, "is_market");
	int i_num_found = get_col_num(res, "num_found");
	int i_settlement_amount = get_col_num(res, "settlement_amount");
	int i_settlement_cash_due_date
			= get_col_num(res, "settlement_cash_due_date");
	int i_settlement_cash_type = get_col_num(res, "settlement_cash_type");
	int i_trade_history_dts = get_col_num(res, "trade_history_dts");
	int i_trade_history_status_id
			= get_col_num(res, "trade_history_status_id");
	int i_trade_price = get_col_num(res, "trade_price");

	pOut->num_found = atoi(PQgetvalue(res, 0, i_num_found));

	vector<string> vAux;

	TokenizeSmart(PQgetvalue(res, 0, i_bid_price), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].bid_price = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_amount), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].cash_transaction_amount = atof(vAux[i].c_str());
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_dts), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[i].cash_transaction_dts.year,
				&pOut->trade_info[i].cash_transaction_dts.month,
				&pOut->trade_info[i].cash_transaction_dts.day,
				&pOut->trade_info[i].cash_transaction_dts.hour,
				&pOut->trade_info[i].cash_transaction_dts.minute,
				&pOut->trade_info[i].cash_transaction_dts.second);
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].cash_transaction_name, vAux[i].c_str(),
				cCT_NAME_len);
		pOut->trade_info[i].cash_transaction_name[cCT_NAME_len] = '\0';
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_exec_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].exec_name, vAux[i].c_str(),
				cEXEC_NAME_len);
		pOut->trade_info[i].exec_name[cEXEC_NAME_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_is_cash), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].is_cash = atoi(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_is_market), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].is_market = atoi(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_amount), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].settlement_amount = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_cash_due_date), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[i].settlement_cash_due_date.year,
				&pOut->trade_info[i].settlement_cash_due_date.month,
				&pOut->trade_info[i].settlement_cash_due_date.day,
				&pOut->trade_info[i].settlement_cash_due_date.hour,
				&pOut->trade_info[i].settlement_cash_due_date.minute,
				&pOut->trade_info[i].settlement_cash_due_date.second);
	}
	if (!check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__)) {
		cout << "*** settlement_cash_due_date = "
			 << PQgetvalue(res, 0, i_settlement_cash_due_date) << endl;
	}
	vAux.clear();
	TokenizeSmart(PQgetvalue(res, 0, i_settlement_cash_type), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].settlement_cash_type, vAux[i].c_str(),
				cSE_CASH_TYPE_len);
		pOut->trade_info[i].settlement_cash_type[cSE_CASH_TYPE_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_history_dts), vAux);
	for (size_t i = 0, k = 0; i < vAux.size() && k < TradeLookupFrame1MaxRows;
			++i, ++k) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[0].year,
				&pOut->trade_info[k].trade_history_dts[0].month,
				&pOut->trade_info[k].trade_history_dts[0].day,
				&pOut->trade_info[k].trade_history_dts[0].hour,
				&pOut->trade_info[k].trade_history_dts[0].minute,
				&pOut->trade_info[k].trade_history_dts[0].second);
		++i;
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[1].year,
				&pOut->trade_info[k].trade_history_dts[1].month,
				&pOut->trade_info[k].trade_history_dts[1].day,
				&pOut->trade_info[k].trade_history_dts[1].hour,
				&pOut->trade_info[k].trade_history_dts[1].minute,
				&pOut->trade_info[k].trade_history_dts[1].second);
		++i;
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[2].year,
				&pOut->trade_info[k].trade_history_dts[2].month,
				&pOut->trade_info[k].trade_history_dts[2].day,
				&pOut->trade_info[k].trade_history_dts[2].hour,
				&pOut->trade_info[k].trade_history_dts[2].minute,
				&pOut->trade_info[k].trade_history_dts[2].second);
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_history_status_id), vAux);
	for (size_t i = 0, k = 0; i < vAux.size() && k < TradeLookupFrame1MaxRows;
			++i, ++k) {
		strncpy(pOut->trade_info[k].trade_history_status_id[0],
				vAux[i].c_str(), cTH_ST_ID_len);
		++i;
		strncpy(pOut->trade_info[k].trade_history_status_id[1],
				vAux[i].c_str(), cTH_ST_ID_len);
		++i;
		strncpy(pOut->trade_info[k].trade_history_status_id[2],
				vAux[i].c_str(), cTH_ST_ID_len);
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_price), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].trade_price = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeLookupFrame2Input *pIn, TTradeLookupFrame2Output *pOut)
{
	uint64_t acct_id = htobe64((uint64_t) pIn->acct_id);

	struct tm end_trade = { 0 };
	end_trade.tm_year = pIn->end_trade_dts.year - 1900;
	end_trade.tm_mon = pIn->end_trade_dts.month - 1;
	end_trade.tm_mday = pIn->end_trade_dts.day;
	end_trade.tm_hour = pIn->end_trade_dts.hour - 1;
	end_trade.tm_min = pIn->end_trade_dts.minute;
	end_trade.tm_sec = pIn->end_trade_dts.second;
	uint64_t end_trade_dts
			= htobe64(((uint64_t) mktime(&end_trade) - (uint64_t) 946684800)
					  * (uint64_t) 1000000);

	uint32_t max_trades = htobe32((uint32_t) pIn->max_trades);

	struct tm start_trade = { 0 };
	start_trade.tm_year = pIn->start_trade_dts.year - 1900;
	start_trade.tm_mon = pIn->start_trade_dts.month - 1;
	start_trade.tm_mday = pIn->start_trade_dts.day;
	start_trade.tm_hour = pIn->start_trade_dts.hour - 1;
	start_trade.tm_min = pIn->start_trade_dts.minute;
	start_trade.tm_sec = pIn->start_trade_dts.second;
	uint64_t start_trade_dts
			= htobe64(((uint64_t) mktime(&start_trade) - (uint64_t) 946684800)
					  * (uint64_t) 1000000);

	const char *paramValues[4] = { (char *) &acct_id, (char *) &end_trade_dts,
		(char *) &max_trades, (char *) &start_trade_dts };
	const int paramLengths[4] = { sizeof(uint64_t), sizeof(uint64_t),
		sizeof(uint32_t), sizeof(uint64_t) };
	const int paramFormats[4] = { 1, 1, 1, 1 };

	PGresult *res = exec("SELECT * FROM TradeLookupFrame2($1, $2, $3, $4)", 4,
			NULL, paramValues, paramLengths, paramFormats, 0);

	int i_bid_price = get_col_num(res, "bid_price");
	int i_cash_transaction_amount
			= get_col_num(res, "cash_transaction_amount");
	int i_cash_transaction_dts = get_col_num(res, "cash_transaction_dts");
	int i_cash_transaction_name = get_col_num(res, "cash_transaction_name");
	int i_exec_name = get_col_num(res, "exec_name");
	int i_is_cash = get_col_num(res, "is_cash");
	int i_num_found = get_col_num(res, "num_found");
	int i_settlement_amount = get_col_num(res, "settlement_amount");
	int i_settlement_cash_due_date
			= get_col_num(res, "settlement_cash_due_date");
	int i_settlement_cash_type = get_col_num(res, "settlement_cash_type");
	int i_trade_history_dts = get_col_num(res, "trade_history_dts");
	int i_trade_history_status_id
			= get_col_num(res, "trade_history_status_id");
	int i_trade_list = get_col_num(res, "trade_list");
	int i_trade_price = get_col_num(res, "trade_price");

	pOut->num_found = atoi(PQgetvalue(res, 0, i_num_found));

	vector<string> vAux;

	TokenizeSmart(PQgetvalue(res, 0, i_bid_price), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].bid_price = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_amount), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].cash_transaction_amount = atof(vAux[i].c_str());
	}
	// FIXME: According to spec, this may not match the returned number found?
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_dts), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[i].cash_transaction_dts.year,
				&pOut->trade_info[i].cash_transaction_dts.month,
				&pOut->trade_info[i].cash_transaction_dts.day,
				&pOut->trade_info[i].cash_transaction_dts.hour,
				&pOut->trade_info[i].cash_transaction_dts.minute,
				&pOut->trade_info[i].cash_transaction_dts.second);
	}
	// FIXME: According to spec, this may not match the returned number found?
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].cash_transaction_name, vAux[i].c_str(),
				cCT_NAME_len);
		pOut->trade_info[i].cash_transaction_name[cCT_NAME_len] = '\0';
	}
	// FIXME: According to spec, this may not match the returned number found?
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_exec_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].exec_name, vAux[i].c_str(),
				cEXEC_NAME_len);
		pOut->trade_info[i].exec_name[cEXEC_NAME_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_is_cash), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].is_cash = atoi(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_amount), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].settlement_amount = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_cash_due_date), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[i].settlement_cash_due_date.year,
				&pOut->trade_info[i].settlement_cash_due_date.month,
				&pOut->trade_info[i].settlement_cash_due_date.day,
				&pOut->trade_info[i].settlement_cash_due_date.hour,
				&pOut->trade_info[i].settlement_cash_due_date.minute,
				&pOut->trade_info[i].settlement_cash_due_date.second);
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_cash_type), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].settlement_cash_type, vAux[i].c_str(),
				cSE_CASH_TYPE_len);
		pOut->trade_info[i].settlement_cash_type[cSE_CASH_TYPE_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_history_dts), vAux);
	for (size_t i = 0, k = 0; i < vAux.size() && k < TradeLookupFrame2MaxRows;
			++i, ++k) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[0].year,
				&pOut->trade_info[k].trade_history_dts[0].month,
				&pOut->trade_info[k].trade_history_dts[0].day,
				&pOut->trade_info[k].trade_history_dts[0].hour,
				&pOut->trade_info[k].trade_history_dts[0].minute,
				&pOut->trade_info[k].trade_history_dts[0].second);
		++i;
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[1].year,
				&pOut->trade_info[k].trade_history_dts[1].month,
				&pOut->trade_info[k].trade_history_dts[1].day,
				&pOut->trade_info[k].trade_history_dts[1].hour,
				&pOut->trade_info[k].trade_history_dts[1].minute,
				&pOut->trade_info[k].trade_history_dts[1].second);
		++i;
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[2].year,
				&pOut->trade_info[k].trade_history_dts[2].month,
				&pOut->trade_info[k].trade_history_dts[2].day,
				&pOut->trade_info[k].trade_history_dts[2].hour,
				&pOut->trade_info[k].trade_history_dts[2].minute,
				&pOut->trade_info[k].trade_history_dts[2].second);
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_history_status_id), vAux);
	for (size_t i = 0, k = 0; i < vAux.size() && k < TradeLookupFrame2MaxRows;
			++i, ++k) {
		strncpy(pOut->trade_info[k].trade_history_status_id[0],
				vAux[i].c_str(), cTH_ST_ID_len);
		++i;
		strncpy(pOut->trade_info[k].trade_history_status_id[1],
				vAux[i].c_str(), cTH_ST_ID_len);
		++i;
		strncpy(pOut->trade_info[k].trade_history_status_id[2],
				vAux[i].c_str(), cTH_ST_ID_len);
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_list), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].trade_id = atoll(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_price), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].trade_price = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeLookupFrame3Input *pIn, TTradeLookupFrame3Output *pOut)
{
	struct tm end_trade = { 0 };
	end_trade.tm_year = pIn->end_trade_dts.year - 1900;
	end_trade.tm_mon = pIn->end_trade_dts.month - 1;
	end_trade.tm_mday = pIn->end_trade_dts.day;
	end_trade.tm_hour = pIn->end_trade_dts.hour - 1;
	end_trade.tm_min = pIn->end_trade_dts.minute;
	end_trade.tm_sec = pIn->end_trade_dts.second;
	uint64_t end_trade_dts
			= htobe64(((uint64_t) mktime(&end_trade) - (uint64_t) 946684800)
					  * (uint64_t) 1000000);

	uint64_t max_acct_id = htobe64((uint64_t) pIn->max_acct_id);

	uint32_t max_trades = htobe32((uint32_t) pIn->max_trades);

	struct tm start_trade = { 0 };
	start_trade.tm_year = pIn->start_trade_dts.year - 1900;
	start_trade.tm_mon = pIn->start_trade_dts.month - 1;
	start_trade.tm_mday = pIn->start_trade_dts.day;
	start_trade.tm_hour = pIn->start_trade_dts.hour - 1;
	start_trade.tm_min = pIn->start_trade_dts.minute;
	start_trade.tm_sec = pIn->start_trade_dts.second;
	uint64_t start_trade_dts
			= htobe64(((uint64_t) mktime(&start_trade) - (uint64_t) 946684800)
					  * (uint64_t) 1000000);

	const char *paramValues[5] = { (char *) &end_trade_dts,
		(char *) &max_acct_id, (char *) &max_trades, (char *) &start_trade_dts,
		pIn->symbol };
	const int paramLengths[5] = { sizeof(uint64_t), sizeof(uint64_t),
		sizeof(uint32_t), sizeof(uint64_t), sizeof(char) * (cSYMBOL_len + 1) };
	const int paramFormats[5] = { 1, 1, 1, 1, 0 };

	PGresult *res = exec("SELECT * FROM TradeLookupFrame3($1, $2, $3, $4, $5)",
			5, NULL, paramValues, paramLengths, paramFormats, 0);

	int i_acct_id = get_col_num(res, "acct_id");
	int i_cash_transaction_amount
			= get_col_num(res, "cash_transaction_amount");
	int i_cash_transaction_dts = get_col_num(res, "cash_transaction_dts");
	int i_cash_transaction_name = get_col_num(res, "cash_transaction_name");
	int i_exec_name = get_col_num(res, "exec_name");
	int i_is_cash = get_col_num(res, "is_cash");
	int i_num_found = get_col_num(res, "num_found");
	int i_price = get_col_num(res, "price");
	int i_quantity = get_col_num(res, "quantity");
	int i_settlement_amount = get_col_num(res, "settlement_amount");
	int i_settlement_cash_due_date
			= get_col_num(res, "settlement_cash_due_date");
	int i_settlement_cash_type = get_col_num(res, "settlement_cash_type");
	int i_trade_dts = get_col_num(res, "trade_dts");
	int i_trade_history_dts = get_col_num(res, "trade_history_dts");
	int i_trade_history_status_id
			= get_col_num(res, "trade_history_status_id");
	int i_trade_list = get_col_num(res, "trade_list");
	int i_trade_type = get_col_num(res, "trade_type");

	pOut->num_found = atoi(PQgetvalue(res, 0, i_num_found));

	vector<string> vAux;

	TokenizeSmart(PQgetvalue(res, 0, i_acct_id), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].acct_id = atoll(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_amount), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].cash_transaction_amount = atof(vAux[i].c_str());
	}
	// FIXME: According to spec, this may not match the returned number found?
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_dts), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[i].cash_transaction_dts.year,
				&pOut->trade_info[i].cash_transaction_dts.month,
				&pOut->trade_info[i].cash_transaction_dts.day,
				&pOut->trade_info[i].cash_transaction_dts.hour,
				&pOut->trade_info[i].cash_transaction_dts.minute,
				&pOut->trade_info[i].cash_transaction_dts.second);
	}
	// FIXME: According to spec, this may not match the returned number found?
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].cash_transaction_name, vAux[i].c_str(),
				cCT_NAME_len);
		pOut->trade_info[i].cash_transaction_name[cCT_NAME_len] = '\0';
	}
	// FIXME: According to spec, this may not match the returned number found?
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_exec_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].exec_name, vAux[i].c_str(),
				cEXEC_NAME_len);
		pOut->trade_info[i].exec_name[cEXEC_NAME_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_is_cash), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].is_cash = atoi(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_price), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].price = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_quantity), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].quantity = atoi(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_amount), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].settlement_amount = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_cash_due_date), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[i].settlement_cash_due_date.year,
				&pOut->trade_info[i].settlement_cash_due_date.month,
				&pOut->trade_info[i].settlement_cash_due_date.day,
				&pOut->trade_info[i].settlement_cash_due_date.hour,
				&pOut->trade_info[i].settlement_cash_due_date.minute,
				&pOut->trade_info[i].settlement_cash_due_date.second);
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_cash_type), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].settlement_cash_type, vAux[i].c_str(),
				cSE_CASH_TYPE_len);
		pOut->trade_info[i].settlement_cash_type[cSE_CASH_TYPE_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_dts), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[i].trade_dts.year,
				&pOut->trade_info[i].trade_dts.month,
				&pOut->trade_info[i].trade_dts.day,
				&pOut->trade_info[i].trade_dts.hour,
				&pOut->trade_info[i].trade_dts.minute,
				&pOut->trade_info[i].trade_dts.second);
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_history_dts), vAux);
	for (size_t i = 0, k = 0; i < vAux.size() && k < TradeLookupFrame3MaxRows;
			++i, ++k) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[0].year,
				&pOut->trade_info[k].trade_history_dts[0].month,
				&pOut->trade_info[k].trade_history_dts[0].day,
				&pOut->trade_info[k].trade_history_dts[0].hour,
				&pOut->trade_info[k].trade_history_dts[0].minute,
				&pOut->trade_info[k].trade_history_dts[0].second);
		++i;
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[1].year,
				&pOut->trade_info[k].trade_history_dts[1].month,
				&pOut->trade_info[k].trade_history_dts[1].day,
				&pOut->trade_info[k].trade_history_dts[1].hour,
				&pOut->trade_info[k].trade_history_dts[1].minute,
				&pOut->trade_info[k].trade_history_dts[1].second);
		++i;
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[2].year,
				&pOut->trade_info[k].trade_history_dts[2].month,
				&pOut->trade_info[k].trade_history_dts[2].day,
				&pOut->trade_info[k].trade_history_dts[2].hour,
				&pOut->trade_info[k].trade_history_dts[2].minute,
				&pOut->trade_info[k].trade_history_dts[2].second);
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_history_status_id), vAux);
	for (size_t i = 0, k = 0; i < vAux.size() && k < TradeLookupFrame3MaxRows;
			++i, ++k) {
		strncpy(pOut->trade_info[k].trade_history_status_id[0],
				vAux[i].c_str(), cTH_ST_ID_len);
		++i;
		strncpy(pOut->trade_info[k].trade_history_status_id[1],
				vAux[i].c_str(), cTH_ST_ID_len);
		++i;
		strncpy(pOut->trade_info[k].trade_history_status_id[2],
				vAux[i].c_str(), cTH_ST_ID_len);
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_list), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].trade_id = atoll(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_type), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].trade_type, vAux[i].c_str(), cTT_ID_len);
		pOut->trade_info[i].trade_type[cTT_ID_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeLookupFrame4Input *pIn, TTradeLookupFrame4Output *pOut)
{
	uint64_t acct_id = htobe64((uint64_t) pIn->acct_id);

	struct tm tm = { 0 };
	tm.tm_year = pIn->trade_dts.year - 1900;
	tm.tm_mon = pIn->trade_dts.month - 1;
	tm.tm_mday = pIn->trade_dts.day;
	tm.tm_hour = pIn->trade_dts.hour - 1;
	tm.tm_min = pIn->trade_dts.minute;
	tm.tm_sec = pIn->trade_dts.second;
	uint64_t trade_dts
			= htobe64(((uint64_t) mktime(&tm) - (uint64_t) 946684800)
					  * (uint64_t) 1000000);

	const char *paramValues[2] = { (char *) &acct_id, (char *) &trade_dts };
	const int paramLengths[2] = { sizeof(uint64_t), sizeof(uint64_t) };
	const int paramFormats[2] = { 1, 1 };

	PGresult *res = exec("SELECT * FROM TradeLookupFrame4($1, $2)", 2, NULL,
			paramValues, paramLengths, paramFormats, 0);

	int i_holding_history_id = get_col_num(res, "holding_history_id");
	int i_holding_history_trade_id
			= get_col_num(res, "holding_history_trade_id");
	int i_num_found = get_col_num(res, "num_found");
	int i_num_trades_found = get_col_num(res, "num_trades_found");
	int i_quantity_after = get_col_num(res, "quantity_after");
	int i_quantity_before = get_col_num(res, "quantity_before");
	int i_trade_id = get_col_num(res, "trade_id");

	pOut->num_found = atoi(PQgetvalue(res, 0, i_num_found));
	pOut->num_trades_found = atoi(PQgetvalue(res, 0, i_num_trades_found));

	vector<string> vAux;

	TokenizeSmart(PQgetvalue(res, 0, i_holding_history_id), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].holding_history_id = atoll(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_holding_history_trade_id), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].holding_history_trade_id = atoll(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_quantity_after), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].quantity_after = atoi(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_quantity_before), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].quantity_before = atoi(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	pOut->trade_id = atoll(PQgetvalue(res, 0, i_trade_id));
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeOrderFrame1Input *pIn, TTradeOrderFrame1Output *pOut)
{
	uint64_t acct_id = htobe64((uint64_t) pIn->acct_id);

	const char *paramValues[1] = { (char *) &acct_id };
	const int paramLengths[1] = { sizeof(uint64_t) };
	const int paramFormats[1] = { 1 };

	PGresult *res = exec("SELECT * FROM TradeOrderFrame1($1)", 1, NULL,
			paramValues, paramLengths, paramFormats, 0);

	int i_acct_name = get_col_num(res, "acct_name");
	int i_broker_id = get_col_num(res, "broker_id");
	int i_broker_name = get_col_num(res, "broker_name");
	int i_cust_f_name = get_col_num(res, "cust_f_name");
	int i_cust_id = get_col_num(res, "cust_id");
	int i_cust_l_name = get_col_num(res, "cust_l_name");
	int i_cust_tier = get_col_num(res, "cust_tier");
	int i_num_found = get_col_num(res, "num_found");
	int i_tax_id = get_col_num(res, "tax_id");
	int i_tax_status = get_col_num(res, "tax_status");

	strncpy(pOut->acct_name, PQgetvalue(res, 0, i_acct_name), cCA_NAME_len);
	pOut->acct_name[cCA_NAME_len] = '\0';
	pOut->broker_id = atoll(PQgetvalue(res, 0, i_broker_id));
	strncpy(pOut->broker_name, PQgetvalue(res, 0, i_broker_name), cB_NAME_len);
	pOut->broker_name[cB_NAME_len] = '\0';
	strncpy(pOut->cust_f_name, PQgetvalue(res, 0, i_cust_f_name), cF_NAME_len);
	pOut->cust_f_name[cF_NAME_len] = '\0';
	pOut->cust_id = atoll(PQgetvalue(res, 0, i_cust_id));
	strncpy(pOut->cust_l_name, PQgetvalue(res, 0, i_cust_l_name), cL_NAME_len);
	pOut->cust_l_name[cL_NAME_len] = '\0';
	pOut->cust_tier = atoi(PQgetvalue(res, 0, i_cust_tier));
	pOut->num_found = atoi(PQgetvalue(res, 0, i_num_found));
	strncpy(pOut->tax_id, PQgetvalue(res, 0, i_tax_id), cTAX_ID_len);
	pOut->tax_id[cTAX_ID_len] = '\0';
	pOut->tax_status = atoi(PQgetvalue(res, 0, i_tax_status));
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeOrderFrame2Input *pIn, TTradeOrderFrame2Output *pOut)
{
	uint64_t acct_id = htobe64((uint64_t) pIn->acct_id);

	const char *paramValues[4] = { (char *) &acct_id, pIn->exec_f_name,
		pIn->exec_l_name, pIn->exec_tax_id };
	const int paramLengths[4] = { sizeof(uint64_t),
		sizeof(char) * (cF_NAME_len + 1), sizeof(char) * (cL_NAME_len + 1),
		sizeof(char) * (cTAX_ID_len + 1) };
	const int paramFormats[4] = { 1, 0, 0, 0 };

	PGresult *res = exec("SELECT * FROM TradeOrderFrame2($1, $2, $3, $4)", 4,
			NULL, paramValues, paramLengths, paramFormats, 0);

	if (PQgetvalue(res, 0, 0) != NULL) {
		strncpy(pOut->ap_acl, PQgetvalue(res, 0, 0), cACL_len);
		pOut->ap_acl[cACL_len] = '\0';
	} else {
		pOut->ap_acl[0] = '\0';
	}
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeOrderFrame3Input *pIn, TTradeOrderFrame3Output *pOut)
{
	uint64_t acct_id = htobe64((uint64_t) pIn->acct_id);
	uint64_t cust_id = htobe64((uint64_t) pIn->cust_id);
	uint16_t cust_tier = htobe16((uint16_t) pIn->cust_tier);
	uint16_t is_lifo = htobe16((uint16_t) pIn->is_lifo);
	uint16_t tax_status = htobe16((uint16_t) pIn->tax_status);
	uint32_t trade_qty = htobe32((uint32_t) pIn->trade_qty);
	uint16_t type_is_margin = htobe16((uint16_t) pIn->type_is_margin);
	char requested_price[14];
	snprintf(requested_price, 13, "%f", pIn->requested_price);

	const char *paramValues[14] = { (char *) &acct_id, (char *) &cust_id,
		(char *) &cust_tier, (char *) &is_lifo, pIn->issue, pIn->st_pending_id,
		pIn->st_submitted_id, (char *) &tax_status, (char *) &trade_qty,
		pIn->trade_type_id, (char *) &type_is_margin, pIn->co_name,
		requested_price, pIn->symbol };
	const int paramLengths[14] = { sizeof(uint64_t), sizeof(uint64_t),
		sizeof(uint16_t), sizeof(uint16_t), sizeof(char) * (cS_ISSUE_len + 1),
		sizeof(char) * (cST_ID_len + 1), sizeof(char) * (cST_ID_len + 1),
		sizeof(uint16_t), sizeof(uint32_t), sizeof(char) * (cTT_ID_len + 1),
		sizeof(uint16_t), sizeof(char) * (cCO_NAME_len + 1), sizeof(char) * 14,
		sizeof(char) * (cSYMBOL_len + 1) };
	const int paramFormats[14] = { 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0 };

	PGresult *res = exec("SELECT * FROM TradeOrderFrame3($1, $2, $3, $4, $5, "
						 "$6, $7, $8, $9, $10, $11, $12, $13, $14)",
			14, NULL, paramValues, paramLengths, paramFormats, 0);

	if (PQntuples(res) == 0) {
		return;
	}

	int i_co_name = get_col_num(res, "co_name");
	int i_requested_price = get_col_num(res, "requested_price");
	int i_symbol = get_col_num(res, "symbol");
	int i_buy_value = get_col_num(res, "buy_value");
	int i_charge_amount = get_col_num(res, "charge_amount");
	int i_comm_rate = get_col_num(res, "comm_rate");
	int i_acct_assets = get_col_num(res, "acct_assets");
	int i_market_price = get_col_num(res, "market_price");
	int i_s_name = get_col_num(res, "s_name");
	int i_sell_value = get_col_num(res, "sell_value");
	int i_status_id = get_col_num(res, "status_id");
	int i_tax_amount = get_col_num(res, "tax_amount");
	int i_type_is_market = get_col_num(res, "type_is_market");
	int i_type_is_sell = get_col_num(res, "type_is_sell");

	strncpy(pOut->co_name, PQgetvalue(res, 0, i_co_name), cCO_NAME_len);
	pOut->requested_price = atof(PQgetvalue(res, 0, i_requested_price));
	strncpy(pOut->symbol, PQgetvalue(res, 0, i_symbol), cSYMBOL_len);
	pOut->symbol[cSYMBOL_len] = '\0';
	pOut->buy_value = atof(PQgetvalue(res, 0, i_buy_value));
	pOut->charge_amount = atof(PQgetvalue(res, 0, i_charge_amount));
	pOut->comm_rate = atof(PQgetvalue(res, 0, i_comm_rate));
	pOut->acct_assets = atof(PQgetvalue(res, 0, i_acct_assets));
	pOut->market_price = atof(PQgetvalue(res, 0, i_market_price));
	strncpy(pOut->s_name, PQgetvalue(res, 0, i_s_name), cS_NAME_len);
	pOut->s_name[cS_NAME_len] = '\0';
	pOut->sell_value = atof(PQgetvalue(res, 0, i_sell_value));
	strncpy(pOut->status_id, PQgetvalue(res, 0, i_status_id), cTH_ST_ID_len);
	pOut->status_id[cTH_ST_ID_len] = '\0';
	pOut->tax_amount = atof(PQgetvalue(res, 0, i_tax_amount));
	pOut->type_is_market = atoi(PQgetvalue(res, 0, i_type_is_market));
	pOut->type_is_sell = atoi(PQgetvalue(res, 0, i_type_is_sell));
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeOrderFrame4Input *pIn, TTradeOrderFrame4Output *pOut)
{
	uint64_t acct_id = htobe64((uint64_t) pIn->acct_id);
	uint64_t broker_id = htobe64((uint64_t) pIn->broker_id);
	char charge_amount[14];
	snprintf(charge_amount, 13, "%f", pIn->charge_amount);
	char comm_amount[14];
	snprintf(comm_amount, 13, "%f", pIn->comm_amount);
	uint16_t is_cash = htobe16((uint16_t) pIn->is_cash);
	uint16_t is_lifo = htobe16((uint16_t) pIn->is_lifo);
	char requested_price[14];
	snprintf(requested_price, 13, "%f", pIn->requested_price);
	uint32_t trade_qty = htobe32((uint32_t) pIn->trade_qty);
	uint16_t type_is_market = htobe16((uint16_t) pIn->type_is_market);

	const char *paramValues[13] = { (char *) &acct_id, (char *) &broker_id,
		charge_amount, comm_amount, pIn->exec_name, (char *) &is_cash,
		(char *) &is_lifo, requested_price, pIn->status_id, pIn->symbol,
		(char *) &trade_qty, pIn->trade_type_id, (char *) &type_is_market };

	const int paramLengths[13] = { sizeof(uint64_t), sizeof(uint64_t),
		sizeof(char) * 14, sizeof(char) * 14,
		sizeof(char) * (cEXEC_NAME_len + 1), sizeof(uint16_t),
		sizeof(uint16_t), sizeof(char) * 14, sizeof(char) * (cST_ID_len + 1),
		sizeof(char) * (cSYMBOL_len + 1), sizeof(uint32_t),
		sizeof(char) * (cTT_ID_len + 1), sizeof(uint16_t) };

	const int paramFormats[13] = { 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1 };

	PGresult *res = exec("SELECT * FROM TradeOrderFrame4($1, $2, $3, $4, $5, "
						 "$6, $7, $8, $9, $10, $11, $12, $13)",
			13, NULL, paramValues, paramLengths, paramFormats, 0);

	pOut->trade_id = atoll(PQgetvalue(res, 0, 0));
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeResultFrame1Input *pIn, TTradeResultFrame1Output *pOut)
{
	uint64_t trade_id = htobe64((uint64_t) pIn->trade_id);

	const char *paramValues[1] = { (char *) &trade_id };
	const int paramLengths[1] = { sizeof(uint64_t) };
	const int paramFormats[1] = { 1 };

	PGresult *res = exec("SELECT * FROM TradeResultFrame1($1)", 1, NULL,
			paramValues, paramLengths, paramFormats, 0);

	int i_acct_id = get_col_num(res, "acct_id");
	int i_charge = get_col_num(res, "charge");
	int i_hs_qty = get_col_num(res, "hs_qty");
	int i_is_lifo = get_col_num(res, "is_lifo");
	int i_num_found = get_col_num(res, "num_found");
	int i_symbol = get_col_num(res, "symbol");
	int i_trade_is_cash = get_col_num(res, "trade_is_cash");
	int i_trade_qty = get_col_num(res, "trade_qty");
	int i_type_id = get_col_num(res, "type_id");
	int i_type_is_market = get_col_num(res, "type_is_market");
	int i_type_is_sell = get_col_num(res, "type_is_sell");
	int i_type_name = get_col_num(res, "type_name");

	pOut->acct_id = atoll(PQgetvalue(res, 0, i_acct_id));
	pOut->charge = atof(PQgetvalue(res, 0, i_charge));
	pOut->hs_qty = atoi(PQgetvalue(res, 0, i_hs_qty));
	pOut->is_lifo = atoi(PQgetvalue(res, 0, i_is_lifo));
	pOut->num_found = atoi(PQgetvalue(res, 0, i_num_found));
	strncpy(pOut->symbol, PQgetvalue(res, 0, i_symbol), cSYMBOL_len);
	pOut->symbol[cSYMBOL_len] = '\0';
	pOut->trade_is_cash = atoi(PQgetvalue(res, 0, i_trade_is_cash));
	pOut->trade_qty = atoi(PQgetvalue(res, 0, i_trade_qty));
	strncpy(pOut->type_id, PQgetvalue(res, 0, i_type_id), cTT_ID_len);
	pOut->type_id[cTT_ID_len] = '\0';
	pOut->type_is_market = atoi(PQgetvalue(res, 0, i_type_is_market));
	pOut->type_is_sell = atoi(PQgetvalue(res, 0, i_type_is_sell));
	strncpy(pOut->type_name, PQgetvalue(res, 0, i_type_name), cTT_NAME_len);
	pOut->type_name[cTT_NAME_len] = '\0';
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeResultFrame2Input *pIn, TTradeResultFrame2Output *pOut)
{
	uint64_t acct_id = htobe64((uint64_t) pIn->acct_id);
	uint32_t hs_qty = htobe32((uint32_t) pIn->hs_qty);
	uint16_t is_lifo = htobe16((uint16_t) pIn->is_lifo);
	uint64_t trade_id = htobe64((uint64_t) pIn->trade_id);
	char trade_price[14];
	snprintf(trade_price, 13, "%f", pIn->trade_price);
	uint32_t trade_qty = htobe32((uint32_t) pIn->trade_qty);
	uint16_t type_is_sell = htobe16((uint16_t) pIn->type_is_sell);

	const char *paramValues[8] = { (char *) &acct_id, (char *) &hs_qty,
		(char *) &is_lifo, pIn->symbol, (char *) &trade_id, trade_price,
		(char *) &trade_qty, (char *) &type_is_sell };

	const int paramLengths[8] = { sizeof(uint64_t), sizeof(uint32_t),
		sizeof(uint16_t), sizeof(char) * (cSYMBOL_len + 1), sizeof(uint64_t),
		sizeof(char) * 14, sizeof(uint32_t), sizeof(uint16_t) };

	const int paramFormats[8] = { 1, 1, 1, 0, 1, 0, 1, 1 };

	PGresult *res = exec("SELECT * FROM TradeResultFrame2($1, $2, $3, $4, $5, "
						 "$6, $7, $8)",
			8, NULL, paramValues, paramLengths, paramFormats, 0);

	pOut->broker_id = atoll(PQgetvalue(res, 0, 0));
	pOut->buy_value = atof(PQgetvalue(res, 0, 1));
	pOut->cust_id = atoll(PQgetvalue(res, 0, 2));
	pOut->sell_value = atof(PQgetvalue(res, 0, 3));
	pOut->tax_status = atoi(PQgetvalue(res, 0, 4));
	sscanf(PQgetvalue(res, 0, 5), "%hd-%hd-%hd %hd:%hd:%hd.%*d",
			&pOut->trade_dts.year, &pOut->trade_dts.month,
			&pOut->trade_dts.day, &pOut->trade_dts.hour,
			&pOut->trade_dts.minute, &pOut->trade_dts.second);
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeResultFrame3Input *pIn, TTradeResultFrame3Output *pOut)
{
	char buy_value[14];
	snprintf(buy_value, 13, "%f", pIn->buy_value);
	uint64_t cust_id = htobe64((uint64_t) pIn->cust_id);
	char sell_value[14];
	snprintf(sell_value, 13, "%f", pIn->sell_value);
	uint64_t trade_id = htobe64((uint64_t) pIn->trade_id);

	const char *paramValues[4]
			= { buy_value, (char *) &cust_id, sell_value, (char *) &trade_id };
	const int paramLengths[4] = { sizeof(char) * 14, sizeof(uint64_t),
		sizeof(char) * 14, sizeof(uint64_t) };
	const int paramFormats[4] = { 0, 1, 0, 1 };

	PGresult *res = exec("SELECT * FROM TradeResultFrame3($1, $2, $3, $4)", 4,
			NULL, paramValues, paramLengths, paramFormats, 0);

	pOut->tax_amount = atof(PQgetvalue(res, 0, 0));
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeResultFrame4Input *pIn, TTradeResultFrame4Output *pOut)
{
	uint64_t cust_id = htobe64((uint64_t) pIn->cust_id);
	uint32_t trade_qty = htobe32((uint32_t) pIn->trade_qty);

	const char *paramValues[4] = { (char *) &cust_id, pIn->symbol,
		(char *) &trade_qty, pIn->type_id };
	const int paramLengths[4]
			= { sizeof(uint64_t), sizeof(char) * (cSYMBOL_len + 1),
				  sizeof(uint32_t), sizeof(char) * (cTT_ID_len + 1) };
	const int paramFormats[4] = { 1, 0, 1, 0 };

	PGresult *res = exec("SELECT * FROM TradeResultFrame4($1, $2, $3, $4)", 4,
			NULL, paramValues, paramLengths, paramFormats, 0);

	pOut->comm_rate = atof(PQgetvalue(res, 0, 0));
	strncpy(pOut->s_name, PQgetvalue(res, 0, 1), cS_NAME_len);
	pOut->s_name[cS_NAME_len] = '\0';
	PQclear(res);
}

void
CDBConnectionServerSide::execute(const TTradeResultFrame5Input *pIn)
{
	uint64_t broker_id = htobe64((uint64_t) pIn->broker_id);
	char comm_amount[14];
	snprintf(comm_amount, 13, "%f", pIn->comm_amount);
	struct tm tm = { 0 };
	tm.tm_year = pIn->trade_dts.year - 1900;
	tm.tm_mon = pIn->trade_dts.month - 1;
	tm.tm_mday = pIn->trade_dts.day;
	tm.tm_hour = pIn->trade_dts.hour - 1;
	tm.tm_min = pIn->trade_dts.minute;
	tm.tm_sec = pIn->trade_dts.second;
	uint64_t trade_dts
			= htobe64(((uint64_t) mktime(&tm) - (uint64_t) 946684800)
					  * (uint64_t) 1000000);

	uint64_t trade_id = htobe64((uint64_t) pIn->trade_id);
	char trade_price[14];
	snprintf(trade_price, 13, "%f", pIn->trade_price);

	const char *paramValues[6]
			= { (char *) &broker_id, comm_amount, pIn->st_completed_id,
				  (char *) &trade_dts, (char *) &trade_id, trade_price };
	const int paramLengths[6] = { sizeof(uint64_t), sizeof(char) * 14,
		sizeof(char) * (cST_ID_len + 1), sizeof(uint64_t), sizeof(uint64_t),
		sizeof(char) * 14 };
	const int paramFormats[6] = { 1, 0, 0, 1, 1, 0 };

	PGresult *res
			= exec("SELECT * FROM TradeResultFrame5($1, $2, $3, $4, $5, $6)",
					6, NULL, paramValues, paramLengths, paramFormats, 0);
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeResultFrame6Input *pIn, TTradeResultFrame6Output *pOut)
{
	uint64_t acct_id = htobe64((uint64_t) pIn->acct_id);
	struct tm due_date_tm = { 0 };
	due_date_tm.tm_year = pIn->due_date.year - 1900;
	due_date_tm.tm_mon = pIn->due_date.month - 1;
	due_date_tm.tm_mday = pIn->due_date.day;
	due_date_tm.tm_hour = pIn->due_date.hour - 1;
	due_date_tm.tm_min = pIn->due_date.minute;
	due_date_tm.tm_sec = pIn->due_date.second;
	uint64_t due_date
			= htobe64(((uint64_t) mktime(&due_date_tm) - (uint64_t) 946684800)
					  * (uint64_t) 1000000);
	char se_amount[14];
	snprintf(se_amount, 13, "%f", pIn->se_amount);
	struct tm trade_dts_tm = { 0 };
	trade_dts_tm.tm_year = pIn->trade_dts.year - 1900;
	trade_dts_tm.tm_mon = pIn->trade_dts.month - 1;
	trade_dts_tm.tm_mday = pIn->trade_dts.day;
	trade_dts_tm.tm_hour = pIn->trade_dts.hour - 1;
	trade_dts_tm.tm_min = pIn->trade_dts.minute;
	trade_dts_tm.tm_sec = pIn->trade_dts.second;
	uint64_t trade_dts
			= htobe64(((uint64_t) mktime(&trade_dts_tm) - (uint64_t) 946684800)
					  * (uint64_t) 1000000);
	uint64_t trade_id = htobe64((uint64_t) pIn->trade_id);
	uint16_t trade_is_cash = htobe16((uint16_t) pIn->trade_is_cash);
	uint32_t trade_qty = htobe32((uint32_t) pIn->trade_qty);

	const char *paramValues[9] = { (char *) &acct_id, (char *) &due_date,
		pIn->s_name, se_amount, (char *) &trade_dts, (char *) &trade_id,
		(char *) &trade_is_cash, (char *) &trade_qty, pIn->type_name };
	const int paramLengths[9] = { sizeof(uint64_t), sizeof(uint64_t),
		sizeof(char) * (cS_NAME_len + 1), sizeof(char) * 14, sizeof(uint64_t),
		sizeof(uint64_t), sizeof(uint16_t), sizeof(uint32_t),
		sizeof(char) * (cTT_NAME_len + 1) };
	const int paramFormats[9] = { 1, 1, 0, 0, 1, 1, 1, 1, 0 };

	PGresult *res = exec("SELECT * FROM TradeResultFrame6($1, $2, $3, $4, $5, "
						 "$6, $7, $8, $9)",
			9, NULL, paramValues, paramLengths, paramFormats, 0);

	pOut->acct_bal = atof(PQgetvalue(res, 0, 0));
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeStatusFrame1Input *pIn, TTradeStatusFrame1Output *pOut)
{
	uint64_t acct_id = htobe64((uint64_t) pIn->acct_id);

	const char *paramValues[1] = { (char *) &acct_id };
	const int paramLengths[1] = { sizeof(uint64_t) };
	const int paramFormats[1] = { 1 };

	PGresult *res = exec("SELECT * FROM TradeStatusFrame1($1)", 1, NULL,
			paramValues, paramLengths, paramFormats, 0);

	int i_broker_name = get_col_num(res, "broker_name");
	int i_charge = get_col_num(res, "charge");
	int i_cust_f_name = get_col_num(res, "cust_f_name");
	int i_cust_l_name = get_col_num(res, "cust_l_name");
	int i_ex_name = get_col_num(res, "ex_name");
	int i_exec_name = get_col_num(res, "exec_name");
	int i_num_found = get_col_num(res, "num_found");
	int i_s_name = get_col_num(res, "s_name");
	int i_status_name = get_col_num(res, "status_name");
	int i_symbol = get_col_num(res, "symbol");
	int i_trade_dts = get_col_num(res, "trade_dts");
	int i_trade_id = get_col_num(res, "trade_id");
	int i_trade_qty = get_col_num(res, "trade_qty");
	int i_type_name = get_col_num(res, "type_name");

	vector<string> vAux;

	pOut->num_found = atoi(PQgetvalue(res, 0, i_num_found));

	strncpy(pOut->broker_name, PQgetvalue(res, 0, i_broker_name), cB_NAME_len);
	pOut->broker_name[cB_NAME_len] = '\0';

	TokenizeSmart(PQgetvalue(res, 0, i_charge), vAux);
	int len = vAux.size();
	for (size_t i = 0; i < (size_t) len; ++i) {
		pOut->charge[i] = atof(vAux[i].c_str());
	}
	vAux.clear();

	strncpy(pOut->cust_f_name, PQgetvalue(res, 0, i_cust_f_name), cF_NAME_len);
	pOut->cust_f_name[cF_NAME_len] = '\0';
	strncpy(pOut->cust_l_name, PQgetvalue(res, 0, i_cust_l_name), cL_NAME_len);
	pOut->cust_l_name[cL_NAME_len] = '\0';

	TokenizeSmart(PQgetvalue(res, 0, i_ex_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->ex_name[i], vAux[i].c_str(), cEX_NAME_len);
		pOut->ex_name[i][cEX_NAME_len] = '\0';
	}
	check_count(len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_exec_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->exec_name[i], vAux[i].c_str(), cEXEC_NAME_len);
		pOut->exec_name[i][cEXEC_NAME_len] = '\0';
	}
	check_count(len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_s_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->s_name[i], vAux[i].c_str(), cS_NAME_len);
		pOut->s_name[i][cS_NAME_len] = '\0';
	}
	check_count(len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_status_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->status_name[i], vAux[i].c_str(), cST_NAME_len);
		pOut->status_name[i][cST_NAME_len] = '\0';
	}
	check_count(len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();
	TokenizeSmart(PQgetvalue(res, 0, i_symbol), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->symbol[i], vAux[i].c_str(), cSYMBOL_len);
		pOut->symbol[i][cSYMBOL_len] = '\0';
	}
	check_count(len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_dts), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_dts[i].year, &pOut->trade_dts[i].month,
				&pOut->trade_dts[i].day, &pOut->trade_dts[i].hour,
				&pOut->trade_dts[i].minute, &pOut->trade_dts[i].second);
	}
	check_count(len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_id), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_id[i] = atoll(vAux[i].c_str());
	}
	check_count(len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_qty), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_qty[i] = atoi(vAux[i].c_str());
	}
	check_count(len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_type_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->type_name[i], vAux[i].c_str(), cTT_NAME_len);
		pOut->type_name[i][cTT_NAME_len] = '\0';
	}
	check_count(len, vAux.size(), __FILE__, __LINE__);
	vAux.clear();
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeUpdateFrame1Input *pIn, TTradeUpdateFrame1Output *pOut)
{
	ostringstream osTrades;
	int i = 0;
	osTrades << "{" << pIn->trade_id[i];
	for (i = 1; i < pIn->max_trades; i++) {
		osTrades << "," << pIn->trade_id[i];
	}
	osTrades << "}";

	uint32_t max_trades = htobe32((uint32_t) pIn->max_trades);
	char trade_id[osTrades.str().length() + 1];
	strncpy(trade_id, osTrades.str().c_str(), osTrades.str().length());
	trade_id[osTrades.str().length()] = '\0';
	uint32_t max_updates = htobe32((uint32_t) pIn->max_updates);

	const char *paramValues[3]
			= { (char *) &max_trades, (char *) &max_updates, trade_id };
	const int paramLengths[3] = { sizeof(uint32_t), sizeof(uint32_t),
		(int) sizeof(char) * ((int) strlen(trade_id) + 1) };
	const int paramFormats[3] = { 1, 1, 0 };

	PGresult *res = exec("SELECT * FROM TradeUpdateFrame1($1, $2, $3)", 3,
			NULL, paramValues, paramLengths, paramFormats, 0);

	int i_bid_price = get_col_num(res, "bid_price");
	int i_cash_transaction_amount
			= get_col_num(res, "cash_transaction_amount");
	int i_cash_transaction_dts = get_col_num(res, "cash_transaction_dts");
	int i_cash_transaction_name = get_col_num(res, "cash_transaction_name");
	int i_exec_name = get_col_num(res, "exec_name");
	int i_is_cash = get_col_num(res, "is_cash");
	int i_is_market = get_col_num(res, "is_market");
	int i_num_found = get_col_num(res, "num_found");
	int i_num_updated = get_col_num(res, "num_updated");
	int i_settlement_amount = get_col_num(res, "settlement_amount");
	int i_settlement_cash_due_date
			= get_col_num(res, "settlement_cash_due_date");
	int i_settlement_cash_type = get_col_num(res, "settlement_cash_type");
	int i_trade_history_dts = get_col_num(res, "trade_history_dts");
	int i_trade_history_status_id
			= get_col_num(res, "trade_history_status_id");
	int i_trade_price = get_col_num(res, "trade_price");

	pOut->num_found = atoi(PQgetvalue(res, 0, i_num_found));

	vector<string> vAux;

	TokenizeSmart(PQgetvalue(res, 0, i_bid_price), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].bid_price = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_amount), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].cash_transaction_amount = atof(vAux[i].c_str());
	}
	// FIXME: According to spec, this may not match the returned number found?
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_dts), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[i].cash_transaction_dts.year,
				&pOut->trade_info[i].cash_transaction_dts.month,
				&pOut->trade_info[i].cash_transaction_dts.day,
				&pOut->trade_info[i].cash_transaction_dts.hour,
				&pOut->trade_info[i].cash_transaction_dts.minute,
				&pOut->trade_info[i].cash_transaction_dts.second);
	}
	// FIXME: According to spec, this may not match the returned number found?
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].cash_transaction_name, vAux[i].c_str(),
				cCT_NAME_len);
		pOut->trade_info[i].cash_transaction_name[cCT_NAME_len] = '\0';
	}
	// FIXME: According to spec, this may not match the returned number found?
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_exec_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].exec_name, vAux[i].c_str(),
				cEXEC_NAME_len);
		pOut->trade_info[i].exec_name[cEXEC_NAME_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();
	TokenizeSmart(PQgetvalue(res, 0, i_is_cash), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].is_cash = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_is_market), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].is_market = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_amount), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].settlement_amount = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_cash_due_date), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[i].settlement_cash_due_date.year,
				&pOut->trade_info[i].settlement_cash_due_date.month,
				&pOut->trade_info[i].settlement_cash_due_date.day,
				&pOut->trade_info[i].settlement_cash_due_date.hour,
				&pOut->trade_info[i].settlement_cash_due_date.minute,
				&pOut->trade_info[i].settlement_cash_due_date.second);
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_cash_type), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].settlement_cash_type, vAux[i].c_str(),
				cSE_CASH_TYPE_len);
		pOut->trade_info[i].settlement_cash_type[cSE_CASH_TYPE_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	pOut->num_updated = atoi(PQgetvalue(res, 0, i_num_updated));

	TokenizeSmart(PQgetvalue(res, 0, i_trade_history_dts), vAux);
	for (size_t i = 0, k = 0; i < vAux.size() && k < TradeUpdateFrame1MaxRows;
			++i, ++k) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[0].year,
				&pOut->trade_info[k].trade_history_dts[0].month,
				&pOut->trade_info[k].trade_history_dts[0].day,
				&pOut->trade_info[k].trade_history_dts[0].hour,
				&pOut->trade_info[k].trade_history_dts[0].minute,
				&pOut->trade_info[k].trade_history_dts[0].second);
		++i;
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[1].year,
				&pOut->trade_info[k].trade_history_dts[1].month,
				&pOut->trade_info[k].trade_history_dts[1].day,
				&pOut->trade_info[k].trade_history_dts[1].hour,
				&pOut->trade_info[k].trade_history_dts[1].minute,
				&pOut->trade_info[k].trade_history_dts[1].second);
		++i;
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[2].year,
				&pOut->trade_info[k].trade_history_dts[2].month,
				&pOut->trade_info[k].trade_history_dts[2].day,
				&pOut->trade_info[k].trade_history_dts[2].hour,
				&pOut->trade_info[k].trade_history_dts[2].minute,
				&pOut->trade_info[k].trade_history_dts[2].second);
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_history_status_id), vAux);
	for (size_t i = 0, k = 0; i < vAux.size() && k < TradeUpdateFrame1MaxRows;
			++i, ++k) {
		strncpy(pOut->trade_info[k].trade_history_status_id[0],
				vAux[i].c_str(), cTH_ST_ID_len);
		++i;
		strncpy(pOut->trade_info[k].trade_history_status_id[1],
				vAux[i].c_str(), cTH_ST_ID_len);
		++i;
		strncpy(pOut->trade_info[k].trade_history_status_id[2],
				vAux[i].c_str(), cTH_ST_ID_len);
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_price), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].trade_price = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeUpdateFrame2Input *pIn, TTradeUpdateFrame2Output *pOut)
{
	uint64_t acct_id = htobe64((uint64_t) pIn->acct_id);
	struct tm end_trade = { 0 };
	end_trade.tm_year = pIn->end_trade_dts.year - 1900;
	end_trade.tm_mon = pIn->end_trade_dts.month - 1;
	end_trade.tm_mday = pIn->end_trade_dts.day;
	end_trade.tm_hour = pIn->end_trade_dts.hour - 1;
	end_trade.tm_min = pIn->end_trade_dts.minute;
	end_trade.tm_sec = pIn->end_trade_dts.second;
	uint64_t end_trade_dts
			= htobe64(((uint64_t) mktime(&end_trade) - (uint64_t) 946684800)
					  * (uint64_t) 1000000);
	uint32_t max_trades = htobe32((uint32_t) pIn->max_trades);
	uint32_t max_updates = htobe32((uint32_t) pIn->max_updates);
	struct tm start_trade = { 0 };
	start_trade.tm_year = pIn->start_trade_dts.year - 1900;
	start_trade.tm_mon = pIn->start_trade_dts.month - 1;
	start_trade.tm_mday = pIn->start_trade_dts.day;
	start_trade.tm_hour = pIn->start_trade_dts.hour - 1;
	start_trade.tm_min = pIn->start_trade_dts.minute;
	start_trade.tm_sec = pIn->start_trade_dts.second;
	uint64_t start_trade_dts
			= htobe64(((uint64_t) mktime(&start_trade) - (uint64_t) 946684800)
					  * (uint64_t) 1000000);

	const char *paramValues[5] = { (char *) &acct_id, (char *) &end_trade_dts,
		(char *) &max_trades, (char *) &max_updates,
		(char *) &start_trade_dts };
	const int paramLengths[5] = { sizeof(uint64_t), sizeof(uint64_t),
		sizeof(uint32_t), sizeof(uint32_t), sizeof(uint64_t) };
	const int paramFormats[5] = { 1, 1, 1, 1, 1 };

	PGresult *res = exec("SELECT * FROM TradeUpdateFrame2($1, $2, $3, $4, $5)",
			5, NULL, paramValues, paramLengths, paramFormats, 0);

	int i_bid_price = get_col_num(res, "bid_price");
	int i_cash_transaction_amount
			= get_col_num(res, "cash_transaction_amount");
	int i_cash_transaction_dts = get_col_num(res, "cash_transaction_dts");
	int i_cash_transaction_name = get_col_num(res, "cash_transaction_name");
	int i_exec_name = get_col_num(res, "exec_name");
	int i_is_cash = get_col_num(res, "is_cash");
	int i_num_found = get_col_num(res, "num_found");
	int i_num_updated = get_col_num(res, "num_updated");
	int i_settlement_amount = get_col_num(res, "settlement_amount");
	int i_settlement_cash_due_date
			= get_col_num(res, "settlement_cash_due_date");
	int i_settlement_cash_type = get_col_num(res, "settlement_cash_type");
	int i_trade_history_dts = get_col_num(res, "trade_history_dts");
	int i_trade_history_status_id
			= get_col_num(res, "trade_history_status_id");
	int i_trade_list = get_col_num(res, "trade_list");
	int i_trade_price = get_col_num(res, "trade_price");

	pOut->num_found = atoi(PQgetvalue(res, 0, i_num_found));

	vector<string> vAux;

	TokenizeSmart(PQgetvalue(res, 0, i_bid_price), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].bid_price = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_amount), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].cash_transaction_amount = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_dts), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[i].cash_transaction_dts.year,
				&pOut->trade_info[i].cash_transaction_dts.month,
				&pOut->trade_info[i].cash_transaction_dts.day,
				&pOut->trade_info[i].cash_transaction_dts.hour,
				&pOut->trade_info[i].cash_transaction_dts.minute,
				&pOut->trade_info[i].cash_transaction_dts.second);
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].cash_transaction_name, vAux[i].c_str(),
				cCT_NAME_len);
		pOut->trade_info[i].cash_transaction_name[cCT_NAME_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_exec_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].exec_name, vAux[i].c_str(),
				cEXEC_NAME_len);
		pOut->trade_info[i].exec_name[cEXEC_NAME_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_is_cash), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].is_cash = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	pOut->num_updated = atoi(PQgetvalue(res, 0, i_num_updated));

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_amount), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].settlement_amount = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_cash_due_date), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[i].settlement_cash_due_date.year,
				&pOut->trade_info[i].settlement_cash_due_date.month,
				&pOut->trade_info[i].settlement_cash_due_date.day,
				&pOut->trade_info[i].settlement_cash_due_date.hour,
				&pOut->trade_info[i].settlement_cash_due_date.minute,
				&pOut->trade_info[i].settlement_cash_due_date.second);
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_cash_type), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].settlement_cash_type, vAux[i].c_str(),
				cSE_CASH_TYPE_len);
		pOut->trade_info[i].settlement_cash_type[cSE_CASH_TYPE_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_history_dts), vAux);
	for (size_t i = 0, k = 0; i < vAux.size() && k < TradeUpdateFrame2MaxRows;
			++i, ++k) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[0].year,
				&pOut->trade_info[k].trade_history_dts[0].month,
				&pOut->trade_info[k].trade_history_dts[0].day,
				&pOut->trade_info[k].trade_history_dts[0].hour,
				&pOut->trade_info[k].trade_history_dts[0].minute,
				&pOut->trade_info[k].trade_history_dts[0].second);
		++i;
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[1].year,
				&pOut->trade_info[k].trade_history_dts[1].month,
				&pOut->trade_info[k].trade_history_dts[1].day,
				&pOut->trade_info[k].trade_history_dts[1].hour,
				&pOut->trade_info[k].trade_history_dts[1].minute,
				&pOut->trade_info[k].trade_history_dts[1].second);
		++i;
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[2].year,
				&pOut->trade_info[k].trade_history_dts[2].month,
				&pOut->trade_info[k].trade_history_dts[2].day,
				&pOut->trade_info[k].trade_history_dts[2].hour,
				&pOut->trade_info[k].trade_history_dts[2].minute,
				&pOut->trade_info[k].trade_history_dts[2].second);
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_history_status_id), vAux);
	for (size_t i = 0, k = 0; i < vAux.size() && k < TradeUpdateFrame2MaxRows;
			++i, ++k) {
		strncpy(pOut->trade_info[k].trade_history_status_id[0],
				vAux[i].c_str(), cTH_ST_ID_len);
		++i;
		strncpy(pOut->trade_info[k].trade_history_status_id[1],
				vAux[i].c_str(), cTH_ST_ID_len);
		++i;
		strncpy(pOut->trade_info[k].trade_history_status_id[2],
				vAux[i].c_str(), cTH_ST_ID_len);
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_list), vAux);
	this->bh = bh;
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].trade_id = atoll(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_price), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].trade_price = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();
	PQclear(res);
}

void
CDBConnectionServerSide::execute(
		const TTradeUpdateFrame3Input *pIn, TTradeUpdateFrame3Output *pOut)
{
	struct tm end_trade = { 0 };
	end_trade.tm_year = pIn->end_trade_dts.year - 1900;
	end_trade.tm_mon = pIn->end_trade_dts.month - 1;
	end_trade.tm_mday = pIn->end_trade_dts.day;
	end_trade.tm_hour = pIn->end_trade_dts.hour - 1;
	end_trade.tm_min = pIn->end_trade_dts.minute;
	end_trade.tm_sec = pIn->end_trade_dts.second;
	uint64_t end_trade_dts
			= htobe64(((uint64_t) mktime(&end_trade) - (uint64_t) 946684800)
					  * (uint64_t) 1000000);
	uint64_t max_acct_id = htobe64((uint64_t) pIn->max_acct_id);
	uint32_t max_trades = htobe32((uint32_t) pIn->max_trades);
	uint32_t max_updates = htobe32((uint32_t) pIn->max_updates);
	struct tm start_trade = { 0 };
	start_trade.tm_year = pIn->start_trade_dts.year - 1900;
	start_trade.tm_mon = pIn->start_trade_dts.month - 1;
	start_trade.tm_mday = pIn->start_trade_dts.day;
	start_trade.tm_hour = pIn->start_trade_dts.hour - 1;
	start_trade.tm_min = pIn->start_trade_dts.minute;
	start_trade.tm_sec = pIn->start_trade_dts.second;
	uint64_t start_trade_dts
			= htobe64(((uint64_t) mktime(&start_trade) - (uint64_t) 946684800)
					  * (uint64_t) 1000000);

	const char *paramValues[6] = { (char *) &end_trade_dts,
		(char *) &max_acct_id, (char *) &max_trades, (char *) &max_updates,
		(char *) &start_trade_dts, pIn->symbol };
	const int paramLengths[6] = { sizeof(uint64_t), sizeof(uint64_t),
		sizeof(uint32_t), sizeof(uint32_t), sizeof(uint64_t),
		sizeof(char) * (cSYMBOL_len + 1) };
	const int paramFormats[6] = { 1, 1, 1, 1, 1, 0 };

	PGresult *res
			= exec("SELECT * FROM TradeUpdateFrame3($1, $2, $3, $4, $5, $6)",
					6, NULL, paramValues, paramLengths, paramFormats, 0);

	int i_acct_id = get_col_num(res, "acct_id");
	int i_cash_transaction_amount
			= get_col_num(res, "cash_transaction_amount");
	int i_cash_transaction_dts = get_col_num(res, "cash_transaction_dts");
	int i_cash_transaction_name = get_col_num(res, "cash_transaction_name");
	int i_exec_name = get_col_num(res, "exec_name");
	int i_is_cash = get_col_num(res, "is_cash");
	int i_num_found = get_col_num(res, "num_found");
	int i_num_updated = get_col_num(res, "num_updated");
	int i_price = get_col_num(res, "price");
	int i_quantity = get_col_num(res, "quantity");
	int i_s_name = get_col_num(res, "s_name");
	int i_settlement_amount = get_col_num(res, "settlement_amount");
	int i_settlement_cash_due_date
			= get_col_num(res, "settlement_cash_due_date");
	int i_settlement_cash_type = get_col_num(res, "settlement_cash_type");
	int i_trade_dts = get_col_num(res, "trade_dts");
	int i_trade_history_dts = get_col_num(res, "trade_history_dts");
	int i_trade_history_status_id
			= get_col_num(res, "trade_history_status_id");
	int i_trade_list = get_col_num(res, "trade_list");
	int i_type_name = get_col_num(res, "type_name");
	int i_trade_type = get_col_num(res, "trade_type");

	pOut->num_found = atoi(PQgetvalue(res, 0, i_num_found));

	vector<string> vAux;

	TokenizeSmart(PQgetvalue(res, 0, i_acct_id), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].acct_id = atoll(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_amount), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].cash_transaction_amount = atof(vAux[i].c_str());
	}
	// FIXME: According to spec, this may not match the returned number found?
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_dts), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[i].cash_transaction_dts.year,
				&pOut->trade_info[i].cash_transaction_dts.month,
				&pOut->trade_info[i].cash_transaction_dts.day,
				&pOut->trade_info[i].cash_transaction_dts.hour,
				&pOut->trade_info[i].cash_transaction_dts.minute,
				&pOut->trade_info[i].cash_transaction_dts.second);
	}
	// FIXME: According to spec, this may not match the returned number found?
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_cash_transaction_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].cash_transaction_name, vAux[i].c_str(),
				cCT_NAME_len);
		pOut->trade_info[i].cash_transaction_name[cCT_NAME_len] = '\0';
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_exec_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].exec_name, vAux[i].c_str(),
				cEXEC_NAME_len);
		pOut->trade_info[i].exec_name[cEXEC_NAME_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_is_cash), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].is_cash = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	pOut->num_updated = atoi(PQgetvalue(res, 0, i_num_updated));

	TokenizeSmart(PQgetvalue(res, 0, i_price), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].price = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_quantity), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].quantity = atoi(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_s_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].s_name, vAux[i].c_str(), cS_NAME_len);
		pOut->trade_info[i].s_name[cS_NAME_len] = '\0';
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_amount), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].settlement_amount = atof(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_cash_due_date), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[i].settlement_cash_due_date.year,
				&pOut->trade_info[i].settlement_cash_due_date.month,
				&pOut->trade_info[i].settlement_cash_due_date.day,
				&pOut->trade_info[i].settlement_cash_due_date.hour,
				&pOut->trade_info[i].settlement_cash_due_date.minute,
				&pOut->trade_info[i].settlement_cash_due_date.second);
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_settlement_cash_type), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].settlement_cash_type, vAux[i].c_str(),
				cSE_CASH_TYPE_len);
		pOut->trade_info[i].settlement_cash_type[cSE_CASH_TYPE_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_dts), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[i].trade_dts.year,
				&pOut->trade_info[i].trade_dts.month,
				&pOut->trade_info[i].trade_dts.day,
				&pOut->trade_info[i].trade_dts.hour,
				&pOut->trade_info[i].trade_dts.minute,
				&pOut->trade_info[i].trade_dts.second);
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_history_dts), vAux);
	for (size_t i = 0, k = 0; i < vAux.size() && k < TradeUpdateFrame3MaxRows;
			++i, ++k) {
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[0].year,
				&pOut->trade_info[k].trade_history_dts[0].month,
				&pOut->trade_info[k].trade_history_dts[0].day,
				&pOut->trade_info[k].trade_history_dts[0].hour,
				&pOut->trade_info[k].trade_history_dts[0].minute,
				&pOut->trade_info[k].trade_history_dts[0].second);
		++i;
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[1].year,
				&pOut->trade_info[k].trade_history_dts[1].month,
				&pOut->trade_info[k].trade_history_dts[1].day,
				&pOut->trade_info[k].trade_history_dts[1].hour,
				&pOut->trade_info[k].trade_history_dts[1].minute,
				&pOut->trade_info[k].trade_history_dts[1].second);
		++i;
		sscanf(vAux[i].c_str(), "%hd-%hd-%hd %hd:%hd:%hd",
				&pOut->trade_info[k].trade_history_dts[2].year,
				&pOut->trade_info[k].trade_history_dts[2].month,
				&pOut->trade_info[k].trade_history_dts[2].day,
				&pOut->trade_info[k].trade_history_dts[2].hour,
				&pOut->trade_info[k].trade_history_dts[2].minute,
				&pOut->trade_info[k].trade_history_dts[2].second);
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_history_status_id), vAux);
	for (size_t i = 0, k = 0; i < vAux.size() && k < TradeUpdateFrame3MaxRows;
			++i, ++k) {
		strncpy(pOut->trade_info[k].trade_history_status_id[0],
				vAux[i].c_str(), cTH_ST_ID_len);
		++i;
		strncpy(pOut->trade_info[k].trade_history_status_id[1],
				vAux[i].c_str(), cTH_ST_ID_len);
		++i;
		strncpy(pOut->trade_info[k].trade_history_status_id[2],
				vAux[i].c_str(), cTH_ST_ID_len);
	}
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_list), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		pOut->trade_info[i].trade_id = atoll(vAux[i].c_str());
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_type_name), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].type_name, vAux[i].c_str(), cTT_NAME_len);
		pOut->trade_info[i].type_name[cTT_NAME_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();

	TokenizeSmart(PQgetvalue(res, 0, i_trade_type), vAux);
	for (size_t i = 0; i < vAux.size(); ++i) {
		strncpy(pOut->trade_info[i].trade_type, vAux[i].c_str(), cTT_ID_len);
		pOut->trade_info[i].trade_type[cTT_ID_len] = '\0';
	}
	check_count(pOut->num_found, vAux.size(), __FILE__, __LINE__);
	vAux.clear();
	PQclear(res);
}
