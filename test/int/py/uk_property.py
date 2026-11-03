import duckdb

# connect, load httpfs ext, sniff csv
con = duckdb.connect()
print("connected")
con.execute("INSTALL httpfs; LOAD httpfs;")
print("httpfs loaded; now sniffing csv...")
print(con.execute("SELECT * FROM sniff_csv('https://datasets-documentation.s3.eu-west-3.amazonaws.com/uk-house-prices/csv/house_prices.csv.gz')").fetchall())

create_table_sql="""
    CREATE TABLE IF NOT EXISTS prices AS
    SELECT * FROM read_csv_auto('https://datasets-documentation.s3.eu-west-3.amazonaws.com/uk-house-prices/csv/house_prices.csv.gz', types={})
"""
schema = {
    'price':'INTEGER',
    'date':'INTEGER',
    'type':'INTEGER',
    'is_new': 'INTEGER',
    'duration': 'INTEGER'
}
sql = create_table_sql.format(schema)
print("Executing %s" % sql)
con.execute(sql)
print("create table done")
# Verify the data
print(con.execute("SELECT COUNT(*) FROM prices").fetchall())
print(con.execute("SELECT * FROM prices limit 10").fetchall())
