-- This file is released under the terms of the Artistic License.  Please see
-- the file LICENSE, included in this package, for details.
--
-- Copyright The DBT-5 Authors
--
-- Based on TPC-E Standard Specification Revision 1.14.0.

-- Clause 3.3.1.3

CREATE OR REPLACE FUNCTION BrokerVolumeFrame1 (
    IN broker_list VARCHAR(100)[40] -- 输入: 经纪人列表
  , IN sector_name VARCHAR(30)      -- 输入: 行业名称
  , OUT broker_name VARCHAR(100)[]  -- 输出: 经纪人名称
  , OUT list_len INTEGER            -- 输出: 经纪人列表的长度
  , OUT volume S_PRICE_T[]          -- 输出: 交易量
) RETURNS RECORD                    -- 返回: RECORD 类型
AS $$
DECLARE
    r RECORD;
BEGIN
    -- 初始化输出参数
    list_len := 0;
    broker_name := '{}';
    volume := '{}';
    FOR r IN
        SELECT b_name   -- 经纪人名称
             , sum(tr_qty * tr_bid_price) AS vol    -- 交易量
        FROM trade_request
           , sector
           , industry
           , company
           , broker
           , security
        WHERE tr_b_id = b_id -- trade_request 中的经纪人ID与 broker 中的经纪人ID匹配
          AND tr_s_symb = s_symb -- trade_request 中的证券代码与 security 中的证券代码匹配
          AND s_co_id = co_id -- security 中的公司ID与 company 中的公司ID匹配
          AND co_in_id = in_id -- company 中的行业ID与 industry 中的行业ID匹配
          AND sc_id = in_sc_id -- sector 中的行业ID与 industry 中的行业ID匹配
          AND b_name = ANY (broker_list) -- broker 中的经纪人名称在 broker_list 中
          AND sc_name = sector_name -- sector 中的行业名称与 sector_name 匹配
        GROUP BY b_name
        ORDER BY 2 DESC -- 按查询结果第 2 列的交易量降序排序
    LOOP
        list_len := list_len + 1;
        broker_name[list_len] = r.b_name;
        volume[list_len] = r.vol;
    END LOOP;
END;
$$
LANGUAGE 'plpgsql';
