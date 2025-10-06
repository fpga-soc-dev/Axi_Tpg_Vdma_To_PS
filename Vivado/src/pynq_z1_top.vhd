library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
library unisim;
use unisim.vcomponents.all;

----------------------------------------------------------------------------------------------------
-- entity declaration
----------------------------------------------------------------------------------------------------
entity pynq_z1_top is
  port (
    -- PS MIO Pins
    FIXED_IO_mio      : inout std_logic_vector(53 downto 0);
    FIXED_IO_ddr_vrn  : inout std_logic;
    FIXED_IO_ddr_vrp  : inout std_logic;
    FIXED_IO_ps_srstb : inout std_logic;
    FIXED_IO_ps_clk   : inout std_logic;
    FIXED_IO_ps_porb  : inout std_logic;
    DDR_cas_n         : inout std_logic;
    DDR_cke           : inout std_logic;
    DDR_ck_n          : inout std_logic;
    DDR_ck_p          : inout std_logic;
    DDR_cs_n          : inout std_logic;
    DDR_reset_n       : inout std_logic;
    DDR_odt           : inout std_logic;
    DDR_ras_n         : inout std_logic;
    DDR_we_n          : inout std_logic;
    DDR_ba            : inout std_logic_vector(2 downto 0);
    DDR_addr          : inout std_logic_vector(14 downto 0);
    DDR_dm            : inout std_logic_vector(3 downto 0);
    DDR_dq            : inout std_logic_vector(31 downto 0);
    DDR_dqs_n         : inout std_logic_vector(3 downto 0);
    DDR_dqs_p         : inout std_logic_vector(3 downto 0);

    -- Switches
    SWITCH1 : in std_logic;
    SWITCH2 : in std_logic;

    -- BUTTONS
    BUTTON1 : in std_logic;
    BUTTON2 : in std_logic;
    BUTTON3 : in std_logic;
    BUTTON4 : in std_logic;

    -- LEDs
    LED1 : out std_logic;
    LED2 : out std_logic;
    LED3 : out std_logic;
    LED4 : out std_logic;

    -- RGB LEDs
    RGBLED1 : out std_logic;
    RGBLED2 : out std_logic;
    RGBLED3 : out std_logic;
    RGBLED4 : out std_logic;
    RGBLED5 : out std_logic;
    RGBLED6 : out std_logic


  );
end pynq_z1_top;

architecture rtl of pynq_z1_top is

  ----------------------------------------------------------------------------------------------------
  -- component declarations
  ----------------------------------------------------------------------------------------------------
  component pynq_z1 is
    port (
      DDR_addr          : inout std_logic_vector (14 downto 0);
      DDR_ba            : inout std_logic_vector (2 downto 0);
      DDR_cas_n         : inout std_logic;
      DDR_ck_n          : inout std_logic;
      DDR_ck_p          : inout std_logic;
      DDR_cke           : inout std_logic;
      DDR_cs_n          : inout std_logic;
      DDR_dm            : inout std_logic_vector (3 downto 0);
      DDR_dq            : inout std_logic_vector (31 downto 0);
      DDR_dqs_n         : inout std_logic_vector (3 downto 0);
      DDR_dqs_p         : inout std_logic_vector (3 downto 0);
      DDR_odt           : inout std_logic;
      DDR_ras_n         : inout std_logic;
      DDR_reset_n       : inout std_logic;
      DDR_we_n          : inout std_logic;
      FIXED_IO_ddr_vrn  : inout std_logic;
      FIXED_IO_ddr_vrp  : inout std_logic;
      FIXED_IO_mio      : inout std_logic_vector (53 downto 0);
      FIXED_IO_ps_clk   : inout std_logic;
      FIXED_IO_ps_porb  : inout std_logic;
      FIXED_IO_ps_srstb : inout std_logic;
      CLK100            : out std_logic;
      CLK_148_5         : out std_logic;
      FCLK_RESET_N      : out std_logic;
      
      LED2               : out std_logic;
      LED3               : out std_logic
      
    );
  end component pynq_z1;
  ----------------------------------------------------------------------------------------------------
  -- signal declarations
  ----------------------------------------------------------------------------------------------------
  signal clk100       : std_logic;
  signal clk148_5     : std_logic;
  signal fclk_reset_n : std_logic;
  signal LedCount     : unsigned(23 downto 0);
  signal LedCount1     : unsigned(23 downto 0);
  

  ----------------------------------------------------------------------------------------------------
  -- attribute declarations
  ----------------------------------------------------------------------------------------------------

begin

  ----------------------------------------------------------------------------------------------------
  -- processor system instance
  ----------------------------------------------------------------------------------------------------
  pynq_z1_i : component pynq_z1
    port map(
      CLK100                    => clk100,
      CLK_148_5                 => clk148_5,
      DDR_addr(14 downto 0)     => DDR_addr(14 downto 0),
      DDR_ba(2 downto 0)        => DDR_ba(2 downto 0),
      DDR_cas_n                 => DDR_cas_n,
      DDR_ck_n                  => DDR_ck_n,
      DDR_ck_p                  => DDR_ck_p,
      DDR_cke                   => DDR_cke,
      DDR_cs_n                  => DDR_cs_n,
      DDR_dm(3 downto 0)        => DDR_dm(3 downto 0),
      DDR_dq(31 downto 0)       => DDR_dq(31 downto 0),
      DDR_dqs_n(3 downto 0)     => DDR_dqs_n(3 downto 0),
      DDR_dqs_p(3 downto 0)     => DDR_dqs_p(3 downto 0),
      DDR_odt                   => DDR_odt,
      DDR_ras_n                 => DDR_ras_n,
      DDR_reset_n               => DDR_reset_n,
      DDR_we_n                  => DDR_we_n,
      FCLK_RESET_N              => fclk_reset_n,
      FIXED_IO_ddr_vrn          => FIXED_IO_ddr_vrn,
      FIXED_IO_ddr_vrp          => FIXED_IO_ddr_vrp,
      FIXED_IO_mio(53 downto 0) => FIXED_IO_mio(53 downto 0),
      FIXED_IO_ps_clk           => FIXED_IO_ps_clk,
      FIXED_IO_ps_porb          => FIXED_IO_ps_porb,
      FIXED_IO_ps_srstb         => FIXED_IO_ps_srstb,
      LED2                      => LED2,
      LED3                      => LED3   
    );
    

    process (clk100)
    begin
      if rising_edge (clk100) then
        if fclk_reset_n = '0' then
          LedCount <= (others => '0');
        else
          LedCount <= LedCount + 1;
        end if;
      end if;
    end process;
    LED1 <= '0' when LedCount(LedCount'high) = '0' else
      '1';
    
    
    process (clk148_5)
    begin
      if rising_edge (clk148_5) then
        if fclk_reset_n = '0' then
          LedCount1 <= (others => '0');
        else
          LedCount1 <= LedCount1 + 1;
        end if;
      end if;
    end process;
    LED4 <= '0' when LedCount1(LedCount1'high) = '0' else
      '1';
      
      

  end rtl;
